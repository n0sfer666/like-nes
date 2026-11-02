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
    print(f"mobile-cpu: PASS demo={hashes['demo']}, idle/cancel/move="
          f"{hashes['idle']}/{hashes['cancel']}/{hashes['move']}, rotation=suspend=cancel, late join ok")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"mobile-cpu: FAIL {error}")
        raise SystemExit(1)
