#!/usr/bin/env python3
"""Run the real local save backend only inside a fresh private directory."""
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv) != 2:
    raise RuntimeError('usage: save_backend_selftest.py SAVE_BACKEND_TEST_EXE')
exe = str(Path(sys.argv[1]).resolve(strict=True))
with tempfile.TemporaryDirectory(prefix='like-nes-save-backend-') as room:
    subprocess.run([exe, room], cwd=room, check=True, timeout=30)
