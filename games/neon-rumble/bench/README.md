# Frame statistics track

`frame-stats.ogg` is original synthetic audio, covered by the repository's dual license.
The source is `generate_track.py`; it uses two quiet triangle waves with a short fade in,
48 kHz stereo, and a phase matched loop from frame 4800 to frame 480000 (exclusive).
Run `python3 generate_track.py` with `oggenc` (generated with vorbis-tools 1.4.3) to regenerate.
The game uses the checked-in Ogg and does not need the encoder.
