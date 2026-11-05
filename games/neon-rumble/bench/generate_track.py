"""Regenerate the original synthetic benchmark track; requires oggenc."""
import io
import struct
import subprocess
import wave
from pathlib import Path


def triangle(frame, period):
    phase = frame % period
    return 2400 - 9600 * min(phase, period - phase) // period


def main():
    # Both periods divide the intro and loop lengths, so the loop joins in phase.
    pcm = bytearray()
    for frame in range(480000):
        gain = min(frame, 4800)
        pcm += struct.pack("<hh", triangle(frame, 160) * gain // 4800,
                           triangle(frame, 240) * gain // 4800)
    wav = io.BytesIO()
    with wave.open(wav, "wb") as sink:
        sink.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        sink.writeframes(pcm)
    output = Path(__file__).with_name("frame-stats.ogg")
    subprocess.run(["oggenc", "--quiet", "--quality", "3", "--serial", "26707",
                    "-o", str(output), "-"], input=wav.getvalue(), check=True)


if __name__ == "__main__":
    main()
