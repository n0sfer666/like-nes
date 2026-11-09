"""Task-owned emulator lifecycle. Every mutating adb command names its new emulator serial."""

import os
from pathlib import Path
import re
import socket
import subprocess
import time

from android_avd_cache import prepare


class Avd:
    def __init__(self, sdk, work, image, cache=None):
        self.sdk, self.work = Path(sdk), Path(work)
        self.cache = Path(cache) if cache else self.work / 'avd-cache'
        if not re.fullmatch(r'system-images;android-(\d+);(?:default|google_apis);(?:x86_64|arm64-v8a)', image):
            raise ValueError('unsupported system image (use a rootable API29+ image)')
        if int(image.split(';')[1][8:]) < 29:
            raise ValueError('the touch gate requires API29+')
        self.image, self.process, self.log = image, None, None
        self.env = dict(os.environ, ANDROID_AVD_HOME=str(self.cache / 'avd'),
                        ANDROID_USER_HOME=str(self.work / 'android-user'),
                        ANDROID_EMULATOR_HOME=str(self.work / 'emulator-user'))
        self.serial, self.lock, self.name = None, None, None
        self.transport, self.ready = None, False

    def alive(self):
        if self.process is None or self.process.poll() is not None:
            raise RuntimeError('task emulator is no longer alive; refusing adb device access')

    def _request(self, args, timeout=15, data=None, binary=False):
        self.alive()
        # ADB forwards only the serial to its emulator-console client.
        selector = ['-t', self.transport] if self.transport and args[0] != 'emu' else ['-s', self.serial]
        result = subprocess.run([str(self.sdk / 'platform-tools/adb'), *selector, *args],
                                input=data, capture_output=True, text=not binary,
                                **({} if binary else {'encoding': 'utf-8', 'errors': 'replace'}),
                                timeout=timeout)
        if result.returncode:
            raise RuntimeError(f'adb {args}: {result.stderr!r}')
        return result.stdout

    def _identity_matches(self):
        if self.transport and self._request(('get-serialno',)).strip() != self.serial:
            return False
        lines = self._request(('emu', 'avd', 'name')).splitlines()
        return bool(lines) and lines[0].strip() == self.name

    def adb(self, *args, **kwargs):
        self.alive()
        if self.ready:
            if not self.transport:
                raise RuntimeError('task emulator lost its pinned adb transport')
            if not self._identity_matches():
                raise RuntimeError('pinned transport has a different AVD identity')
            self.alive()
        return self._request(args, **kwargs)

    def bind_transport(self):
        if self._request(('shell', 'getprop', 'ro.kernel.qemu')).strip() != '1':
            raise RuntimeError('selected serial is not an emulator')
        if not self._identity_matches():
            raise RuntimeError('serial does not belong to the task AVD; refusing device writes')
        transport = self._request(('transport-id',)).strip()
        if not re.fullmatch(r'[1-9][0-9]*', transport):
            raise RuntimeError('adb did not identify a unique transport')
        self.transport = transport
        if not self._identity_matches():
            raise RuntimeError('ADB transport changed while selecting the task AVD')

    def prepare(self):
        self.name, self.lock = prepare(self.cache, self.sdk, self.work, self.image, self.env)

    def _snapshot_marker(self):
        marker = self.cache / '.boot-snapshot.pending'
        if marker.exists() or marker.is_symlink():
            raise ValueError('interrupted task AVD snapshot save; choose a fresh cache path')
        return marker

    def _has_snapshot(self):
        snapshot = self.cache / 'avd/device.avd/snapshots/like-nes-touch/snapshot.pb'
        if not snapshot.exists() and not snapshot.is_symlink():
            return False
        if snapshot.is_symlink() or not snapshot.is_file() or snapshot.stat().st_size == 0:
            raise ValueError('incomplete task AVD boot snapshot; choose a fresh cache path')
        return True

    def start(self):
        self.prepare()
        self._snapshot_marker()
        Path(self.env['ANDROID_USER_HOME']).mkdir(parents=True, exist_ok=True)
        Path(self.env['ANDROID_EMULATOR_HOME']).mkdir(parents=True, exist_ok=True)
        port = None
        for candidate in range(5554, 5680, 2):
            sockets = []
            try:
                for number in (candidate, candidate + 1):
                    sock = socket.socket()
                    sockets.append(sock)
                    sock.bind(('127.0.0.1', number))
                port = candidate
                break
            except OSError:
                continue
            finally:
                for sock in sockets:
                    sock.close()
        if port is None:
            raise RuntimeError('no free emulator console/adb port pair')
        self.serial = f'emulator-{port}'
        args = [str(self.sdk / 'emulator/emulator'), '-avd', self.name, '-port', str(port),
                '-no-window', '-no-audio', '-no-boot-anim', '-gpu', 'swiftshader_indirect',
                '-feature', 'Vulkan', '-no-snapshot-save', '-camera-back', 'none', '-camera-front', 'none']
        args += ['-snapshot', 'like-nes-touch'] if self._has_snapshot() else ['-no-snapshot-load']
        self.log = (self.work / 'emulator.log').open('w', encoding='utf-8')
        self.process = subprocess.Popen(args, stdout=self.log, stderr=subprocess.STDOUT, env=self.env)
        deadline = time.monotonic() + 180
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError(f'emulator exited {self.process.returncode}: {self.work / "emulator.log"}')
            try:
                if self.adb('shell', 'getprop', 'sys.boot_completed', timeout=3).strip() == '1':
                    break
            except (RuntimeError, subprocess.TimeoutExpired):
                pass
            time.sleep(1)
        else:
            raise RuntimeError('task emulator did not boot in 180 seconds')
        self.bind_transport()
        self.adb('root')
        # root reconnects adbd. Re-identify once; after this there is no serial fallback.
        self.transport = None
        self.adb('wait-for-device', timeout=30)
        self.bind_transport()
        self.ready = True
        if self.adb('shell', 'id', '-u').strip() != '0':
            raise RuntimeError('protocol B injection needs a rootable emulator image')

    def save_snapshot(self):
        self.alive()
        if not self.ready or self.lock is None:
            raise RuntimeError('snapshot save needs a live, owned AVD')
        marker = self._snapshot_marker()
        if self._has_snapshot():
            return
        marker.touch(exist_ok=False)
        out = self.adb('emu', 'avd', 'snapshot', 'save', 'like-nes-touch', timeout=45)
        response = out.strip().splitlines()
        if not response or response[-1] != 'OK' or 'KO:' in out or not self._has_snapshot():
            raise RuntimeError(f'cannot save task AVD snapshot: {out}')
        marker.unlink()

    def close(self):
        if self.process is not None and self.process.poll() is None:
            # Signal only our Popen child: never adb-kill a serial that might have been reused.
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=10)
        if self.log:
            self.log.close()
        if self.lock:
            self.lock.close()
        self.process, self.log, self.lock = None, None, None
