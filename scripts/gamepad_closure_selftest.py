"""Проверить настоящее CMake-правило на косвенных оконных рёбрах и цикле."""

from pathlib import Path
import subprocess
import tempfile


def main():
    rule = Path(__file__).resolve().with_name('gamepad_closure.cmake')
    cases = [('plain', 'helper', 'core', True),
             ('private', '$<LINK_ONLY:helper>', 'core', True),
             ('cycle', '$<LINK_ONLY:helper>', 'pad', True),
             ('direct-window', 'glfw', 'core', False),
             ('indirect-window', 'helper', 'glfw', False),
             ('private-window', '$<LINK_ONLY:helper>', 'glfw', False),
             ('private-pump', '$<LINK_ONLY:helper>', '$<LINK_ONLY:input_native>', False)]
    with tempfile.TemporaryDirectory(prefix='gamepad-closure-') as folder:
        for name, edge, nested, good in cases:
            source = Path(folder) / name
            source.mkdir()
            text = ('cmake_minimum_required(VERSION 3.21)\nproject(closure NONE)\n'
                    'include("${RULE}")\n'
                    'foreach(name pad helper core glfw input_native)\n'
                    '  add_library(${name} INTERFACE)\nendforeach()\n'
                    f'target_link_libraries(pad INTERFACE "{edge}")\n'
                    f'target_link_libraries(helper INTERFACE "{nested}")\n'
                    'no_window(pad)\n')
            (source / 'CMakeLists.txt').write_text(text, encoding='utf-8')
            result = subprocess.run(['cmake', '-S', str(source), '-B', str(source / 'build'),
                                     '-G', 'Ninja', f'-DRULE={rule.as_posix()}'],
                                    capture_output=True, text=True, encoding='utf-8', errors='replace')
            log = result.stdout + result.stderr
            if (result.returncode == 0) != good or (not good and 'closure pulls window' not in log):
                raise RuntimeError(f'{name}: unexpected verdict\n{log}')
    print('gamepad-closure: PASS 3 good + 4 broken graphs (plain, LINK_ONLY, cycle)')


if __name__ == '__main__':
    main()
