#!/usr/bin/env python3
"""Owned AVD guards and teardown, without starting adb or an emulator."""

from pathlib import Path
import subprocess
import tempfile
from unittest.mock import Mock, patch

from android_gate_avd import Avd


def selftest():
    image = 'system-images;android-35;google_apis;x86_64'
    with tempfile.TemporaryDirectory(prefix='android-avd-selftest-') as directory:
        root = Path(directory)
        for scenario in ('normal', 'wrong-avd', 'not-qemu', 'crashed'):
            work = root / scenario
            work.mkdir()
            avd = Avd(root / 'sdk', work, image)
            calls = []
            replaced = False

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
                assert command[1:3] == (['-t', avd.transport] if avd.transport else ['-s', avd.serial]), command
                args = command[3:]
                if args == ['shell', 'getprop', 'sys.boot_completed']:
                    out = '1'
                elif args == ['shell', 'getprop', 'ro.kernel.qemu']:
                    out = '0' if scenario == 'not-qemu' else '1'
                elif args == ['emu', 'avd', 'name']:
                    out = ('unrelated-avd' if scenario == 'wrong-avd' or replaced else avd.name) + '\nOK\n'
                elif args == ['transport-id']:
                    out = '88'
                elif args == ['shell', 'id', '-u']:
                    out = '0'
                else:
                    out = 'OK'
                return subprocess.CompletedProcess(command, 0, out, '')

            process = Mock()
            process.poll.return_value = -4 if scenario == 'crashed' else None
            process.returncode = -4
            try:
                with patch('android_gate_avd.subprocess.run', side_effect=run), \
                     patch('android_gate_avd.subprocess.Popen', return_value=process) as popen:
                    try:
                        avd.start()
                        assert scenario == 'normal', f'{scenario} was accepted'
                        avd.save_snapshot()
                        args, kwargs = popen.call_args
                        command = args[0]
                        assert command[command.index('-avd') + 1] == avd.name
                        assert command[command.index('-gpu') + 1] == 'swiftshader_indirect'
                        assert kwargs['env']['ANDROID_AVD_HOME'].startswith(str(work))
                        before = len(calls)
                        process.poll.return_value = -4
                        try:
                            avd.adb('install', '-r', 'task.apk')
                            raise AssertionError('dead child still allowed install')
                        except RuntimeError:
                            assert len(calls) == before
                        process.poll.return_value = None
                        replaced = True
                        try:
                            avd.adb('shell', 'pm', 'clear', 'task')
                            raise AssertionError('replaced transport still allowed clear')
                        except RuntimeError:
                            assert not any(command[3:5] == ['install', '-r'] or command[3:6] ==
                                           ['shell', 'pm', 'clear'] for command in calls)
                        replaced = False
                    except RuntimeError:
                        assert scenario != 'normal'
                    if scenario != 'normal':
                        assert not any(command[3:] == ['root'] for command in calls)
                    avd.close()
                    if scenario != 'crashed':
                        process.terminate.assert_called_once()
            finally:
                avd.close()
        work = root / 'normal'
        cached = Avd(root / 'sdk', work, image)
        try:
            cached.prepare()
            assert cached.name.startswith('like-nes-touch-')
            concurrent = Avd(root / 'sdk', work, image)
            try:
                try:
                    concurrent.prepare()
                    raise AssertionError('concurrent AVD writer was accepted')
                except BlockingIOError:
                    pass
            finally:
                concurrent.close()
        finally:
            cached.close()
        ini = work / 'avd-cache/avd' / (cached.name + '.ini')
        data = work / 'avd-cache/avd/device.avd'
        config = data / 'config.ini'
        original_ini, original_config = ini.read_text(encoding='utf-8'), config.read_text(encoding='utf-8')

        def refuse_cache():
            candidate = Avd(root / 'sdk', work, image)
            try:
                with patch('android_gate_avd.subprocess.Popen', side_effect=AssertionError('unsafe cache launched')):
                    try:
                        candidate.start()
                        raise AssertionError('unsafe cache was accepted')
                    except ValueError:
                        pass
            finally:
                candidate.close()

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
            try:
                unmarked.prepare()
                raise AssertionError('unmarked nonempty directory was accepted')
            except ValueError:
                pass
            assert (work / 'avd-cache/user-file').read_text(encoding='utf-8') == 'keep'
        finally:
            unmarked.close()
        external = root / 'external'
        external.mkdir()
        alias = root / 'cache-alias'
        alias.symlink_to(external, target_is_directory=True)
        linked = Avd(root / 'sdk', work, image, alias)
        try:
            try:
                linked.prepare()
                raise AssertionError('symlink at cache root was accepted')
            except ValueError as error:
                assert 'cache directory must not be a symlink' in str(error)
            assert not list(external.iterdir()), 'cache alias wrote outside the owned directory'
        finally:
            linked.close()
    print('android-avd-selftest: PASS owned/pinned transport, death/replacement after boot, cached lock, own teardown')
    return 0


if __name__ == '__main__':
    raise SystemExit(selftest())
