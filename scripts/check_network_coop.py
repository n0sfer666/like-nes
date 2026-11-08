#!/usr/bin/env python3
"""Run two installed-SDK Neon Rumble processes through local UDP."""
import argparse
from contextlib import ExitStack
import heapq
from pathlib import Path
import re
import selectors
import socket
import subprocess
import time

FRAMES = 180
RESULT = re.compile(r"^network result tick (\d+) hash ([a-f0-9]{16}) rollbacks (\d+) reconnects (\d+) effects (\d+)$",
                    re.MULTILINE)


def delay_for(log):
    delays = re.findall(r"^network lobby started delay ([1-3])$", log, re.MULTILINE)
    assert len(delays) == 1, log
    return int(delays[0])


def same_inputs(reports):
    by_delay = {}
    for delay, digest in reports:
        assert by_delay.setdefault(delay, digest) == digest, "same input delay produced different simulation"


def bind(stack, family, address):
    sock = stack.enter_context(socket.socket(family, socket.SOCK_DGRAM))
    sock.bind((address, 0))
    sock.setblocking(False)
    return sock


class Proxy:
    def __init__(self, stack, host, scenario):
        self.scenario = scenario
        self.host = host
        self.guest = None
        self.sockets = [bind(stack, socket.AF_INET, "127.0.0.1") for _ in range(2)]
        self.selector = stack.enter_context(selectors.DefaultSelector())
        for index, sock in enumerate(self.sockets):
            self.selector.register(sock, selectors.EVENT_READ, index)
        self.queue = []
        self.seq = 0
        self.play_packets = 0
        self.dropped = 0
        self.duplicated = 0
        self.first_play = None
        self.stale = None
        self.replayed = False

    @property
    def address(self):
        return self.sockets[0].getsockname()

    def enqueue(self, now, out, to, data, delay):
        self.seq += 1
        heapq.heappush(self.queue, (now + delay, self.seq, out, to, data))

    def receive(self, index, data, source, now):
        if index == 0:
            self.guest = source
            destination = self.host
        else:
            if source != self.host or self.guest is None:
                return
            destination = self.guest
        out = self.sockets[1 - index]
        if data[:4] != b"PLAY":
            self.enqueue(now, out, destination, data, 0)
            return
        self.play_packets += 1
        if self.first_play is None:
            self.first_play = now
        elapsed = now - self.first_play
        cut = self.scenario in {"recover", "solo"} and index == 0 and elapsed >= 0.75
        if cut and (self.scenario == "solo" or elapsed < 2.0):
            self.dropped += 1
            if self.stale is None:
                self.stale = (out, destination, data)
            return
        if self.scenario == "mixed":
            if self.play_packets % 11 == 0:
                self.dropped += 1
                return
            delay = (0.003, 0.036, 0.014)[self.play_packets % 3]
            if self.play_packets % 13 == 0:
                self.enqueue(now, out, destination, data, delay + 0.005)
                self.duplicated += 1
        else:
            delay = 0
        self.enqueue(now, out, destination, data, delay)

    def pump(self):
        now = time.monotonic()
        for key, _ in self.selector.select(timeout=0.002):
            for _ in range(32):
                try:
                    data, source = key.fileobj.recvfrom(1200)
                except (BlockingIOError, ConnectionResetError):
                    break
                self.receive(key.data, data, source, now)
        if self.scenario == "solo" and self.stale and not self.replayed and now - self.first_play >= 31.6:
            self.enqueue(now, *self.stale, 0)
            self.replayed = True
        while self.queue and self.queue[0][0] <= now:
            _, _, sock, address, data = heapq.heappop(self.queue)
            sock.sendto(data, address)


def endpoint(address):
    host, port = address[:2]
    return f"[{host}]:{port}" if ":" in host else f"{host}:{port}"


def stop(child):
    if child.poll() is None:
        child.kill()
    child.wait()


def run(executable, scenario, family=socket.AF_INET):
    address = "::1" if family == socket.AF_INET6 else "127.0.0.1"
    with ExitStack() as stack:
        reservation = bind(stack, family, address)
        host = reservation.getsockname()
        proxy = Proxy(stack, host, scenario) if scenario != "direct" else None
        guest = proxy.address if proxy else host
        reservation.close()
        children = []
        for role, target in (("host", host), ("join", guest)):
            child = subprocess.Popen([str(executable), f"--net-{role}", endpoint(target), "--frames", str(FRAMES)],
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8")
            stack.enter_context(child)
            stack.callback(stop, child)
            children.append(child)
        deadline = time.monotonic() + (45 if scenario == "solo" else 18)
        while any(child.poll() is None for child in children):
            if time.monotonic() >= deadline:
                for child in children:
                    stop(child)
                output = [child.communicate()[0] for child in children]
                raise AssertionError(f"{scenario} timed out: {output}")
            if proxy:
                proxy.pump()
            else:
                time.sleep(0.002)
        output = [child.communicate()[0] for child in children]
        results = [RESULT.search(log) for log in output]
        if any(child.returncode != 0 for child in children) or any(match is None for match in results):
            raise AssertionError(f"{scenario} did not finish: {output}")
        hashes = []
        for result, log in zip(results, output):
            tick, digest, _, reconnects, effects = result.groups()
            assert int(tick) == int(effects) == FRAMES, log
            if scenario in {"solo", "recover"}:
                assert "network phase paused" in log, log
            if scenario == "recover":
                assert int(reconnects) == 1, log
            if scenario == "solo":
                assert "network phase solo" in log and int(reconnects) == 0, log
            hashes.append(digest)
        if scenario == "mixed":
            assert proxy.dropped > 0 and proxy.duplicated > 0
        if scenario == "solo":
            assert proxy.replayed and hashes[0] != hashes[1], output
        else:
            assert hashes[0] == hashes[1], output
        delay = delay_for(output[0])
        assert delay_for(output[1]) == delay, output
        print(f"network-coop: {scenario} {address}, tick {FRAMES}, delay {delay}, {', '.join(hashes)}", flush=True)
        return delay, hashes[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--solo", action="store_true", help="run the live 30-second one-way outage")
    args = parser.parse_args()
    exe = args.exe.resolve()
    if args.solo:
        run(exe, "solo")
    else:
        cases = (("direct", socket.AF_INET), ("direct", socket.AF_INET6),
                 ("mixed", socket.AF_INET), ("recover", socket.AF_INET))
        same_inputs(run(exe, scenario, family) for scenario, family in cases)
    print("network-coop: PASS")


if __name__ == "__main__":
    main()
