#!/usr/bin/env python3
"""Owned AVD guards and teardown, without starting adb or an emulator."""

from contextlib import closing
from pathlib import Path
import subprocess
import tempfile
from unittest import TestCase
from unittest.mock import Mock, patch

from android_gate_avd import Avd


def selftest():
    image = 'system-images;android-35;google_apis;x86_64'
    expect = TestCase()
    failed_saves = {'missing': ('OK', None), 'empty': ('OK', b''), 'unacknowledged': ('', b'partial')}
    refused_devices = ('wrong-avd', 'not-qemu', 'crashed')
    with tempfile.TemporaryDirectory(prefix='android-avd-selftest-') as directory:
        root = Path(directory)
        for scenario in ('normal', 'warm', *failed_saves, *refused_devices):
            work = root / ('normal' if scenario == 'warm' else scenario)
            work.mkdir(exist_ok=scenario == 'warm')
            avd = Avd(root / 'sdk', work, image)
            snapshot = work / 'avd-cache/avd/device.avd/snapshots/like-nes-touch/snapshot.pb'
            calls = []
            replaced = transport_replaced = False

            def run(command, **kwargs):
                calls.append(command)
                if 'create' in command:
                    name = command[command.index('--name') + 1]
                    data = Path(command[command.index('--path') + 1])
                    data.mkdir()
                    (data / 'config.ini').write_text(f'image.sysdir.1={image.replace(";", "/")}\n'
                                                    'disk.dataPartition.path=<temp>\n', encoding='utf-8')
                    (Path(kwargs['env']['ANDROID_AVD_HOME']) / (name + '.ini')).write_text(
                        f'path={data}\npath.rel=avd/device.avd\n', encoding='utf-8')
                    return subprocess.CompletedProcess(command, 0, '', '')
                args = command[3:]
                selector = ['-t', avd.transport] if avd.transport and args[0] != 'emu' else ['-s', avd.serial]
                assert command[1:3] == selector, command
                if args == ['shell', 'getprop', 'sys.boot_completed']:
                    out = '1'
                elif args == ['shell', 'getprop', 'ro.kernel.qemu']:
                    out = '0' if scenario == 'not-qemu' else '1'
                elif args == ['emu', 'avd', 'name']:
                    out = ('unrelated-avd' if scenario == 'wrong-avd' or replaced else avd.name) + '\nOK\n'
                elif args == ['transport-id']:
                    out = '88'
                elif args == ['get-serialno']:
                    out = 'other-serial' if transport_replaced else avd.serial
                elif args == ['shell', 'id', '-u']:
                    out = '0'
                elif args == ['emu', 'avd', 'snapshot', 'save', 'like-nes-touch']:
                    assert scenario != 'warm', 'overwrote a restored snapshot'
                    out, contents = failed_saves.get(scenario, ('OK', b'boot snapshot'))
                    if contents is not None:
                        snapshot.parent.mkdir(parents=True)
                        snapshot.write_bytes(contents)
                else:
                    out = 'OK'
                return subprocess.CompletedProcess(command, 0, out, '')

            process = Mock()
            process.poll.return_value = -4 if scenario == 'crashed' else None
            process.returncode = -4
            try:
                with patch('android_gate_avd.subprocess.run', side_effect=run), \
                     patch('android_gate_avd.subprocess.Popen', return_value=process) as popen:
                    if scenario in refused_devices:
                        with expect.assertRaises(RuntimeError):
                            avd.start()
                        assert not any(command[3:] == ['root'] for command in calls)
                    else:
                        avd.start()
                    if scenario in failed_saves:
                        with expect.assertRaises((RuntimeError, ValueError)):
                            avd.save_snapshot()
                        avd.close()
                        reopened = Avd(root / 'sdk', work, image)
                        try:
                            with expect.assertRaisesRegex(ValueError, 'interrupted task AVD snapshot save'):
                                reopened.start()
                        finally:
                            reopened.close()
                        popen.assert_called_once()
                    if scenario in ('normal', 'warm'):
                        avd.save_snapshot()
                        args, kwargs = popen.call_args
                        command = args[0]
                        assert command[command.index('-avd') + 1] == avd.name
                        assert command[command.index('-gpu') + 1] == 'swiftshader_indirect'
                        assert ('-snapshot' in command) == (scenario == 'warm')
                        assert ('-no-snapshot-load' in command) == (scenario == 'normal')
                        assert snapshot.is_file()
                        assert kwargs['env']['ANDROID_AVD_HOME'].startswith(str(work))
                        before = len(calls)
                        process.poll.return_value = -4
                        with expect.assertRaises(RuntimeError):
                            avd.adb('install', 'task.apk')
                        assert len(calls) == before
                        process.poll.return_value = None
                        replaced = True
                        with expect.assertRaises(RuntimeError):
                            avd.adb('shell', 'pm', 'clear', 'task')
                        assert not any(command[3:5] == ['install', 'task.apk'] or command[3:6] ==
                                       ['shell', 'pm', 'clear'] for command in calls)
                        replaced = False
                        transport_replaced = True
                        for args in (('uninstall', 'task'), ('emu', 'sensor', 'set', 'acceleration', '0:0:0')):
                            before = len(calls)
                            with expect.assertRaises(RuntimeError):
                                avd.adb(*args)
                            assert [command[3:] for command in calls[before:]] == [['get-serialno']]
                    avd.close()
                    if scenario != 'crashed':
                        process.terminate.assert_called_once()
            finally:
                avd.close()
        work = root / 'normal'
        with closing(Avd(root / 'sdk', work, image)) as cached:
            cached.prepare()
            assert cached.name.startswith('like-nes-touch-')
            with closing(Avd(root / 'sdk', work, image)) as concurrent, expect.assertRaises(BlockingIOError):
                concurrent.prepare()
        ini = work / 'avd-cache/avd' / (cached.name + '.ini')
        data = work / 'avd-cache/avd/device.avd'
        config = data / 'config.ini'
        original_ini, original_config = ini.read_text(encoding='utf-8'), config.read_text(encoding='utf-8')

        def refuse_cache():
            with closing(Avd(root / 'sdk', work, image)) as candidate:
                with patch('android_gate_avd.subprocess.Popen', side_effect=AssertionError('unsafe cache launched')):
                    with expect.assertRaises(ValueError):
                        candidate.start()

        snapshot = data / 'snapshots/like-nes-touch/snapshot.pb'
        snapshot.write_bytes(b'')
        refuse_cache()
        snapshot.write_bytes(b'boot snapshot')
        ini.write_text(original_ini.replace(str(data), str(root / 'unrelated.avd')), encoding='utf-8')
        refuse_cache()
        ini.write_text(original_ini + 'path.rel=../../unrelated.avd\n', encoding='utf-8')
        refuse_cache()
        ini.write_text(original_ini, encoding='utf-8')
        assert f'AvdId={cached.name}\n' in original_config
        partition = f'disk.dataPartition.path={data.resolve() / "userdata-qemu.img"}\n'
        assert partition in original_config
        config.write_text(original_config.replace(f'AvdId={cached.name}\n', ''), encoding='utf-8')
        refuse_cache()
        config.write_text(original_config.replace(f'AvdId={cached.name}\n', 'AvdId=foreign\n'), encoding='utf-8')
        refuse_cache()
        config.write_text(original_config.replace('android-35', 'android-34'), encoding='utf-8')
        refuse_cache()
        config.write_text(original_config.replace(partition, 'disk.dataPartition.path=<temp>\n'), encoding='utf-8')
        refuse_cache()
        config.write_text(original_config.replace(partition, f'disk.dataPartition.path={root}/unrelated.img\n'),
                          encoding='utf-8')
        refuse_cache()
        config.write_text(original_config, encoding='utf-8')
        saved = data.with_name('saved.avd')
        data.rename(saved)
        data.symlink_to(saved, target_is_directory=True)
        refuse_cache()
        data.unlink()
        saved.rename(data)
        (data / 'foreign.img').symlink_to(root / 'unrelated.img')
        refuse_cache()
        (data / 'foreign.img').unlink()
        work = root / 'unmarked'
        (work / 'avd-cache').mkdir(parents=True)
        (work / 'avd-cache/user-file').write_text('keep', encoding='utf-8')
        unmarked = Avd(root / 'sdk', work, image)
        try:
            with expect.assertRaises(ValueError):
                unmarked.prepare()
            assert (work / 'avd-cache/user-file').read_text(encoding='utf-8') == 'keep'
        finally:
            unmarked.close()
        external = root / 'external'
        external.mkdir()
        alias = root / 'cache-alias'
        alias.symlink_to(external, target_is_directory=True)
        linked = Avd(root / 'sdk', work, image, alias)
        try:
            with expect.assertRaisesRegex(ValueError, 'cache directory must not be a symlink'):
                linked.prepare()
            assert not list(external.iterdir()), 'cache alias wrote outside the owned directory'
        finally:
            linked.close()
    print('android-avd-selftest: PASS owned/pinned transport, death/replacement, boot snapshot reuse, cached lock, own teardown')
    return 0


if __name__ == '__main__':
    raise SystemExit(selftest())
