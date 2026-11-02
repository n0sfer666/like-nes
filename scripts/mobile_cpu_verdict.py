#!/usr/bin/env python3
"""Реальный CPU-цикл MobileRumble без поверхности и равенство явного демо десктопу."""
import re
import subprocess
import sys

import py_utf8


def run(args: list[str]) -> str:
    result = subprocess.run(args, capture_output=True, text=True, encoding="utf-8", timeout=60)
    if result.returncode:
        raise RuntimeError(f"{args[0]} exited {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result.stdout + result.stderr


def main() -> int:
    py_utf8.enable()
    if len(sys.argv) != 4:
        print("usage: mobile_cpu_verdict.py <mobile-cpu> <desktop-game> <assets-dir>")
        return 1
    exe, desk, assets = sys.argv[1:]
    hashes = {}
    for mode in ("demo", "idle", "move", "cancel", "rotate", "suspend", "late"):
        text = run([exe, assets, mode])
        if "mobile-cpu: PASS" not in text or f"[game] controls {'demo' if mode == 'demo' else 'touch'}" not in text:
            raise RuntimeError(f"{mode}: no verdict or wrong controls mode\n{text}")
        ticks = 480 if mode == "demo" else 180 if mode == "late" else 120
        found = re.search(rf"surface detached tick={ticks} world=([0-9a-f]{{16}})", text)
        if not found:
            raise RuntimeError(f"{mode}: no world at tick {ticks}\n{text}")
        hashes[mode] = found[1]
        if mode == "late":
            for tick, state in ((61, "joining"), (62, "present")):
                if not re.search(rf"^neon-rumble: seat tick {tick}: P1 {state}$", text, re.M):
                    raise RuntimeError(f"late contact did not join P1 ({state} at {tick})\n{text}")
    desktop = run([desk, "--headless", "--frames", "480"])
    found = re.search(r"brawl tick 480: hash ([0-9a-f]{16})", desktop)
    if not found or hashes["demo"] != found[1]:
        raise RuntimeError("explicit mobile demo differs from desktop at tick 480")
    if len({hashes[mode] for mode in ("idle", "move", "cancel")}) != 3:
        raise RuntimeError(f"mobile touch movement/cancel did not affect simulation: {hashes}")
    if hashes["cancel"] != hashes["rotate"] or hashes["cancel"] != hashes["suspend"]:
        raise RuntimeError("safe-only rotation or suspend did not cancel like explicit cancel")
    wave = run([exe, assets, "wave-demo"])
    desktop_wave = run([desk, "--headless", "--wave", "--frames", "8640"])
    mobile_hash = re.search(r"surface detached tick=8640 world=([0-9a-f]{16})", wave)
    desktop_hash = re.search(r"brawl tick 8640: hash ([0-9a-f]{16})", desktop_wave)
    if not mobile_hash or not desktop_hash or mobile_hash[1] != desktop_hash[1]:
        raise RuntimeError("mobile wave demo differs from desktop at tick 8640")
    for evidence in ("[game] controls demo", "[game] level mode wave", "mobile-cpu: PASS",
                     "arena tick 3736: arena 0 released", "arena tick 7530: arena 1 released"):
        if evidence not in wave:
            raise RuntimeError(f"wave demo lacks {evidence}")
    touch_wave = run([exe, assets, "wave-touch"])
    for evidence in ("[game] controls touch", "[game] level mode wave", "mobile-cpu: PASS",
                     "seat tick 1: P1 joining", "seat tick 2: P1 present",
                     "checkpoint tick 202: order 0 at 600,248", "arena tick 258: arena 0 locked",
                     "wave tick 258: arena 0 wave 0,", "surface detached tick=480"):
        if evidence not in touch_wave:
            raise RuntimeError(f"interactive wave lacks {evidence}")
    print(f"mobile-cpu: PASS demo={hashes['demo']}, idle/cancel/move="
          f"{hashes['idle']}/{hashes['cancel']}/{hashes['move']}, rotation=suspend=cancel, late join ok; "
          f"wave8640={mobile_hash[1]}, interactive wave checkpoint202/arena258")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"mobile-cpu: FAIL {error}")
        raise SystemExit(1)
