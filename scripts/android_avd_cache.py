"""Create or validate a task-owned AVD cache before emulator code can open disk images."""

import fcntl
import json
from pathlib import Path
import re
import subprocess
import uuid


def ini_fields(path):
    fields = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        if not line.strip() or line.lstrip().startswith(('#', ';')):
            continue
        key, value = (part.strip() for part in line.split('=', 1))
        if key in fields:
            raise ValueError(f'duplicate AVD ini key {key}')
        fields[key] = value
    return fields


def validate(cache, sdk, image, name, *, allow_relative=False):
    home, data = cache / 'avd', cache / 'avd/device.avd'
    ini, config = home / (name + '.ini'), data / 'config.ini'
    for path in (home, data, ini, config):
        if path.is_symlink():
            raise ValueError('task AVD path must not be a symlink')
    if not data.is_dir() or not ini.is_file() or not config.is_file():
        raise ValueError('incomplete task AVD cache; choose a fresh cache path')
    for path in data.rglob('*'):
        if path.is_symlink():
            raise ValueError('task AVD data contains a symlink')
    fields = ini_fields(ini)
    configured = Path(fields.get('path', ''))
    if not configured.is_absolute() or configured.resolve() != data.resolve():
        raise ValueError('task AVD ini points outside the owned data directory')
    if not allow_relative and 'path.rel' in fields:
        raise ValueError('task AVD cache must use its verified absolute data path only')
    cfg = ini_fields(config)
    if cfg.get('AvdId') != name:
        raise ValueError('task AVD config has a different identity')
    image_path = Path(cfg.get('image.sysdir.1', ''))
    if not image_path.is_absolute():
        image_path = sdk / image_path
    if image_path.resolve() != (sdk / image.replace(';', '/')).resolve():
        raise ValueError('task AVD config has a different system image')
    for config_path in (config, data / 'hardware-qemu.ini'):
        if not config_path.exists():
            continue
        for key, value in ini_fields(config_path).items():
            writable = (key.startswith('disk.') and key.endswith('.path') and key != 'disk.ramdisk.path')
            if value and (writable or key == 'sdcard.path'):
                path = Path(value)
                if not path.is_absolute() or not path.resolve().is_relative_to(data.resolve()):
                    raise ValueError('task AVD disk path escapes the owned data directory')
            if value and (key.endswith('.initPath') or key in ('disk.ramdisk.path', 'kernel.path')):
                if not Path(value).resolve().is_relative_to(image_path.resolve()):
                    raise ValueError('task AVD boot image path escapes its system image')


def prepare(cache, sdk, work, image, env):
    cache.mkdir(parents=True, exist_ok=True)
    marker = cache / 'like-nes-owned-avd.json'
    if marker.is_symlink():
        raise ValueError('task AVD marker must not be a symlink')
    if not marker.exists() and any(path.name != '.gate.lock' for path in cache.iterdir()):
        raise ValueError('refusing an unmarked/nonempty AVD cache directory')
    if (cache / '.gate.lock').is_symlink():
        raise ValueError('task AVD lock must not be a symlink')
    lock = (cache / '.gate.lock').open('a', encoding='utf-8')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        marker = cache / 'like-nes-owned-avd.json'
        if marker.is_symlink():
            raise ValueError('task AVD marker must not be a symlink')
        if marker.exists():
            identity = json.loads(marker.read_text(encoding='utf-8'))
            name = identity.get('name', '') if isinstance(identity, dict) else ''
            if (not isinstance(identity, dict) or identity.get('schema') != 1 or
                    identity.get('image') != image or not isinstance(name, str) or
                    not re.fullmatch(r'like-nes-touch-[0-9a-f]{12}', name)):
                raise ValueError('AVD cache belongs to a different image/schema; choose a fresh cache path')
            validate(cache, sdk, image, name)
            return name, lock
        if any(path.name != '.gate.lock' for path in cache.iterdir()):
            raise ValueError('refusing an unmarked/nonempty AVD cache directory')
        Path(env['ANDROID_AVD_HOME']).mkdir(parents=True)
        Path(env['ANDROID_USER_HOME']).mkdir(parents=True, exist_ok=True)
        name = 'like-nes-touch-' + uuid.uuid4().hex[:12]
        manager = sdk / 'cmdline-tools/latest/bin/avdmanager'
        result = subprocess.run([str(manager), 'create', 'avd', '--name', name, '--package', image,
                                 '--path', str(cache / 'avd/device.avd'), '--device', 'pixel_4'],
                                input='no\n', capture_output=True, text=True, encoding='utf-8',
                                errors='replace', env=env, timeout=60)
        (work / 'avdmanager.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode:
            raise RuntimeError(f'avdmanager failed: {work / "avdmanager.log"}')
        validate(cache, sdk, image, name, allow_relative=True)
        # Fresh files only: remove the manager's optional fallback to a different Android home.
        ini = cache / 'avd' / (name + '.ini')
        fields = ini_fields(ini)
        fields.pop('path.rel', None)
        ini.write_text(''.join(f'{key}={value}\n' for key, value in fields.items()), encoding='utf-8')
        validate(cache, sdk, image, name)
        marker.write_text(json.dumps({'schema': 1, 'image': image, 'name': name}), encoding='utf-8')
        return name, lock
    except BaseException:
        lock.close()
        raise
