"""Exercise the SDK stand driver without GPU or physical audio."""
import subprocess
import sys
import tempfile
from pathlib import Path

from frame_stats_verdict import report_from_text, validate


def require(condition, explanation):
    if not condition:
        raise ValueError(explanation)


def test_case(exe, mode):
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        target = root / "like-nes-frame-stats"
        if mode == "failed-save":
            target.write_bytes(b"untouched")
        proc = subprocess.run([exe, mode, str(root)], stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, encoding="utf-8", timeout=20)
        require(proc.returncode == 0, f"{mode}: {proc.stdout}")
        if mode == "bad-bundle":
            require("[frame-stats]" not in proc.stdout and not target.exists(), "bad bundle side effects")
            return
        report = report_from_text(proc.stdout)
        complete = mode in ("ok", "env-root")
        expected = {"frames": 4, "intervals": 2, "misses": 1, "menu_frames": 2,
                    "work_p50_ns": 3000000, "work_p99_ns": 4000000,
                    "interval_p50_ns": 33333333, "interval_p99_ns": 33333333,
                    "source": "fixture-null-audio", "capture_ready": True, "audio_ok": True,
                    "save_ok": mode not in ("no-save", "failed-save", "relative-root"), "complete": complete,
                    "touch_move": mode != "no-touch", "touch_attack": mode != "no-touch"}
        for key, value in expected.items():
            require(report.get(key) == value, f"{mode}: {key} != {value!r}: {report!r}")
        require((report["audible_callbacks"] > 0) == (mode != "silent"), f"{mode}: wrong PCM evidence")
        if mode == "failed-save":
            require(target.read_bytes() == b"untouched", "failed save touched an existing file")
        if mode in ("no-save", "relative-root"):
            require(not target.exists(), f"{mode}: unexpected slot creation")
        try:
            validate(report)
        except ValueError:
            require(not complete, "valid fixture report rejected")
        else:
            require(complete, "incomplete fixture report passed")


def main(exe):
    try:
        for mode in ("ok", "no-save", "failed-save", "no-touch", "silent", "bad-bundle", "env-root", "relative-root"):
            test_case(exe, mode)
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        print(f"sdk-frame-stats: FAIL: {error}", file=sys.stderr)
        return 1
    print("sdk-frame-stats: PASS (eight real SDK driver cases, no hardware claim)")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
