# Synthetic Vorbis fixtures

These 0.1 second signals were generated for this project, without external audio.
They use the repository license (MIT OR Apache-2.0). The left and right integer waveforms differ;
48000 Hz is a supported music rate; 44100 Hz and the three-channel source exercise importer refusal.
Tests consume the committed Ogg files; no encoder is required to build or run them.

Generated with vorbis-tools `oggenc 1.4.3`, libVorbis `1.3.7`, using this Python recipe from
the repository root (requires those tools only when deliberately regenerating fixtures):

```python
from pathlib import Path
import struct
import subprocess
import tempfile
import wave

with tempfile.TemporaryDirectory() as directory:
    for rate in (44100, 48000):
        wav = Path(directory) / f"stereo-{rate}.wav"
        with wave.open(str(wav), "wb") as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(rate)
            output.writeframes(b"".join(
                struct.pack("<hh", (i % 37) * 400 - 7200, (i % 53) * 200 - 5200)
                for i in range(rate // 10)
            ))
        subprocess.run([
            "oggenc", "--quiet", "--quality", "3", "--serial", str(26000 + rate),
            "-o", f"tests/audio/fixtures/stereo-{rate}.ogg", str(wav),
        ], check=True)
```

PCM is compared against the same pinned decoder's old one-shot API on each host, not against
a cross-architecture floating-point hash. The reference call runs before concurrent tests;
production consumers use `audio::VorbisReader`, which guards the vendor's global CRC table.

`three-48000.ogg` uses the same tools, quality and duration, with three int16 channels at48000Hz.
The samples are `struct.pack("<hhh", (i % 37) * 400 - 7200, (i % 53) * 200 - 5200,
(i % 29) * 300 - 4200)` for `i in range(4800)`, written with `setnchannels(3)`.
Its encoder serial is74003; the output filename is `three-48000.ogg`. No external audio is used.
