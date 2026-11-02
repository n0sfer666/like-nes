#!/usr/bin/env python3
"""Stale temp links must not truncate any link to the prior save inode."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 2:
        raise RuntimeError('usage: atomic_links_selftest.py SAVE_ATOMIC_TEST_EXE')
    exe = str(Path(sys.argv[1]).resolve(strict=True))
    with tempfile.TemporaryDirectory(prefix='like-nes-atomic-links-') as room:
        root = Path(room)
        for kind in ('hardlink', 'symlink'):
            target = root / kind
            target.write_bytes(b'OLD')
            observer = root / (kind + '.observer')
            os.link(target, observer)
            temp = root / (kind + '.tmp')
            try:
                if kind == 'hardlink':
                    os.link(target, temp)
                else:
                    temp.symlink_to(target)
            except OSError as error:
                if kind == 'symlink' and getattr(error, 'winerror', 0) == 1314:
                    print('atomic-links: SKIP symlink (Windows requires link privilege)')
                    continue
                raise
            subprocess.run([exe, '--write', str(target)], check=True, timeout=20)
            if target.is_symlink() or target.read_bytes() != bytes((9, 8, 7)):
                raise RuntimeError(kind + ': target is not a complete new regular image')
            if observer.read_bytes() != b'OLD' or temp.exists() or temp.is_symlink():
                raise RuntimeError(kind + ': old inode was truncated or temp survived')
            print('atomic-links: PASS ' + kind + ', old inode intact')


if __name__ == '__main__':
    main()
