#!/usr/bin/env python3
"""Exercise a room on a real relay and cut the verified direct route."""
import argparse
from contextlib import ExitStack
from pathlib import Path
import queue
import selectors
import socket
import subprocess
import threading
import time

from check_network_coop import FRAMES, RESULT, bind, delay_for, endpoint, same_inputs, stop


class RouteCut:
    def __init__(self, stack, server, rebind, control_loss):
        self.server = server
        self.sockets = [bind(stack, socket.AF_INET, "127.0.0.1") for _ in range(2)]
        self.selector = stack.enter_context(selectors.DefaultSelector())
        for role, sock in enumerate(self.sockets):
            self.selector.register(sock, selectors.EVENT_READ, role)
        self.backend = bind(stack, socket.AF_INET, "127.0.0.1") if rebind else None
        if self.backend:
            self.selector.register(self.backend, selectors.EVENT_READ, 2)
        self.clients = [None, None]
        self.control_loss = control_loss
        self.first_relay_play = None
        self.rebind_attempts = 0
        self.first_direct = None
        self.dropped = 0

    def address(self, role):
        return endpoint(self.sockets[role].getsockname())

    def pump(self):
        now = time.monotonic()
        cut = self.first_direct is not None and now - self.first_direct > 0.65
        for key, _ in self.selector.select(timeout=0.002):
            for _ in range(32):
                try:
                    data, source = key.fileobj.recvfrom(1200)
                except (BlockingIOError, ConnectionResetError):
                    break
                role = key.data
                if role == 2:
                    if source == self.server and self.clients[0]:
                        self.sockets[0].sendto(data, self.clients[0])
                    continue
                if source == self.server:
                    if self.control_loss and self.first_relay_play and now - self.first_relay_play > 0.25:
                        if data[:4] == b"RLY1" and data[5] in {2, 6}:  # Challenge and State
                            self.dropped += 1
                            continue
                    if self.clients[role] and not (self.backend and role == 0 and cut):
                        key.fileobj.sendto(data, self.clients[role])
                elif data[:4] == b"RLY1":
                    if data[42:46] == b"PLAY" and self.first_relay_play is None:
                        self.first_relay_play = now
                    if data[5:6] == b"\x01" and data[7:8] == b"\x30":
                        self.rebind_attempts += 1
                    self.clients[role] = source
                    via = self.backend if self.backend and role == 0 and cut else key.fileobj
                    via.sendto(data, self.server)
                elif source == self.clients[1 - role] and self.clients[role]:
                    if data[:4] == b"PLAY" and self.first_direct is None:
                        self.first_direct = now
                    if cut:
                        self.dropped += 1
                        continue
                    self.sockets[1 - role].sendto(data, self.clients[role])


def start(stack, argv):
    child = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8")
    stack.enter_context(child)
    stack.callback(stop, child)
    return child


def lines_from(child, out):
    for line in child.stdout:
        out.put(line.rstrip())
    out.put(None)


def until(out, prefix, deadline, pump):
    lines = []
    while time.monotonic() < deadline:
        pump()
        try:
            line = out.get_nowait()
        except queue.Empty:
            continue
        if line is None:
            break
        lines.append(line)
        if line.startswith(prefix):
            return line
    raise AssertionError(f"missing {prefix!r}: {lines}")


def run(exe, relay_address, server=None, rebind=False, control_loss=False):
    with ExitStack() as stack:
        proxy = RouteCut(stack, server, rebind, control_loss) if server else None
        address = proxy.address if proxy else lambda _: relay_address
        pump = proxy.pump if proxy else lambda: time.sleep(0.002)
        direct = proxy and not control_loss
        options = ["--frames", str(FRAMES)] + ([] if direct else ["--relay-only"])
        host = start(stack, [str(exe), "--net-create", address(0), *options])
        out = queue.Queue()
        reader = threading.Thread(target=lines_from, args=(host, out), daemon=True)
        reader.start()
        line = until(out, "network room ", time.monotonic() + 9, pump)
        code = line.split()[-1]
        guest = start(stack, [str(exe), "--net-code", address(1), code, *options])

        deadline = time.monotonic() + 18
        while host.poll() is None or guest.poll() is None:
            if time.monotonic() >= deadline:
                stop(host)
                stop(guest)
                break
            pump()
        reader.join(timeout=2)
        assert not reader.is_alive(), "host log reader did not stop"
        host_log = "\n".join(line for line in list(out.queue) if line is not None)
        guest_log = guest.communicate()[0]
        logs = (host_log, guest_log)
        results = [RESULT.search(log) for log in logs]
        assert host.returncode == guest.returncode == 0 and all(results), logs
        assert results[0][2] == results[1][2], logs
        for log, result in zip(logs, results):
            routes = [line for line in log.splitlines() if line.startswith("network route ")]
            if direct:
                assert routes[:3] == ["network route relay", "network route direct", "network route relay"], log
            else:
                assert routes == ["network route relay"], log
            if rebind:
                assert "network phase paused" in log and int(result[4]) == 1, log
            if control_loss:
                assert "network phase paused" not in log, log
        if proxy:
            assert proxy.dropped > 0, "fault was injected"
        if control_loss:
            assert proxy.rebind_attempts > 0, "rebind ran during control-only loss"
        route = "control loss" if control_loss else "rebind" if rebind else "direct to relay" if direct else "relay"
        delay = delay_for(host_log)
        assert delay_for(guest_log) == delay, logs
        print(f"network-relay: {route} tick {FRAMES}, delay {delay}, hash {results[0][2]}")
        return delay, results[0][2]


def local(exe, daemon):
    with ExitStack() as stack:
        reservation = bind(stack, socket.AF_INET, "127.0.0.1")
        server = reservation.getsockname()
        reservation.close()
        relay = start(stack, [str(daemon), str(server[1]), "--loopback"])
        log = queue.Queue()
        threading.Thread(target=lines_from, args=(relay, log), daemon=True).start()
        until(log, "relay ready ", time.monotonic() + 5, lambda: time.sleep(0.002))
        same_inputs((
            run(exe, endpoint(server)),
            run(exe, endpoint(("::1", server[1]))),
            run(exe, "", server=server),
            run(exe, "", server=server, rebind=True),
            run(exe, "", server=server, control_loss=True),
        ))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--daemon", type=Path)
    target.add_argument("--address", help="running relay endpoint")
    args = parser.parse_args()
    if args.daemon:
        local(args.exe.resolve(), args.daemon.resolve())
    else:
        run(args.exe.resolve(), args.address)
    print("network-relay: PASS")


if __name__ == "__main__":
    main()
