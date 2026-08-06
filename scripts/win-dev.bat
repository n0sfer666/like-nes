@echo off
rem Windows-обёртка: одна точка входа вместо выбора правильного ярлыка в меню Пуск.
rem
rem Развилка «Developer Command Prompt / Developer PowerShell / x64 Native Tools» — источник
rem половины падений на этой ОС, и ни одно из них не называет себя: 32-битный шелл проявляется
rem как C4244 в нашем render/capture.cpp, отсутствующий SDK — как «компилятор не может собрать
rem простую программу». Здесь окружение поднимается программно, из ЛЮБОГО шелла: vswhere находит
rem установку VS, vcvars64.bat даёт заведомо 64-битный тулчейн.
rem
rem   scripts\win-dev.bat setup   winget: Build Tools + Windows SDK + Git + Python
rem   scripts\win-dev.bat build   конфигурирование и сборка (по умолчанию)
rem   scripts\win-dev.bat warn    собрать без -Werror и выписать ВСЕ предупреждения разом
rem   scripts\win-dev.bat check   scripts/owner_check.sh шеллом git-bash
rem   scripts\win-dev.bat gate8   scripts/gate8_e2e.sh тем же шеллом (сквозной гейт 8 спеки #13)
rem   scripts\win-dev.bat probe   framework_input_probe с живым падом (гейт 8 спеки #14)
rem   scripts\win-dev.bat game    запустить собранную игру (2-й аргумент — адаптер: d3d12/vulkan/low)
rem   scripts\win-dev.bat demo    отрисовать 60 кадров в PNG мимо окна (чёрный экран: кто виноват)
rem   scripts\win-dev.bat shell   отдать cmd с готовым x64-окружением
rem   scripts\win-dev.bat clean   удалить каталог build (кеш помнит компилятор)
setlocal EnableExtensions
cd /d "%~dp0.." || exit /b 1

set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=build"

rem Скобка в самом ИМЕНИ переменной закрывает блок раньше времени, если развернуть её внутри
rem `if (...)` или списка `for`. Разворачиваем один раз здесь, на верхнем уровне.
set "PF86=%ProgramFiles(x86)%"

rem Диагностики cl.exe — на языке установки, и в русской они приезжают в OEM-кодировке: лог
rem нечитаем глазами и мимо любого grep по «warning». VSLANG=1033 переводит компилятор на
rem английский; ровно это же делает CI, и парсер диагностик в tools/ide рассчитан на него.
set "VSLANG=1033"

if /i "%ACTION%"=="setup" goto do_setup
if /i "%ACTION%"=="clean" goto do_clean
if /i "%ACTION%"=="build" goto need_vs
if /i "%ACTION%"=="warn"  goto need_vs
if /i "%ACTION%"=="check" goto need_vs
if /i "%ACTION%"=="gate8" goto need_vs
if /i "%ACTION%"=="probe" goto need_vs
if /i "%ACTION%"=="game"  goto need_vs
if /i "%ACTION%"=="demo"  goto need_vs
if /i "%ACTION%"=="shell" goto need_vs
echo win-dev.bat: unknown action "%ACTION%"
echo usage: win-dev.bat [setup^|build^|warn^|check^|gate8^|probe^|game^|demo^|shell^|clean]
exit /b 2

:do_setup
where winget >nul 2>&1
if errorlevel 1 (
    echo winget not found. Install "App Installer" from the Microsoft Store, then rerun.
    exit /b 1
)
rem winget выходит ненулём на «уже установлено», поэтому errorlevel здесь не гейт: реальную
rem проверку делает build, и она честнее — там окружение уже поднято.
echo === Git for Windows ^(git + the POSIX shell the gates need^)
winget install --id Git.Git -e --accept-package-agreements --accept-source-agreements
echo === Python 3 ^(python.org build; the Store stub does not execute^)
winget install --id Python.Python.3.13 -e --accept-package-agreements --accept-source-agreements
rem Build Tools, а не IDE: тот же компилятор без редактора, примерно 5 ГБ против 20+. Компоненты
rem перечислены поимённо — воркладом по умолчанию SDK не гарантирован, а без него нет rc.exe.
echo === Visual Studio Build Tools ^(compiler + Windows SDK, no IDE^)
winget install --id Microsoft.VisualStudio.2022.BuildTools -e ^
    --accept-package-agreements --accept-source-agreements ^
    --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.VC.CMake.Project"
echo.
echo Setup finished. Open a NEW shell ^(PATH changed^) and run: scripts\win-dev.bat build
echo If the Build Tools step failed on the SDK component, the id above is pinned to
echo Windows 11 SDK 26100 - pick the one your machine offers in the Visual Studio Installer.
exit /b 0

:do_clean
if exist build (
    echo Removing build\ ...
    rmdir /s /q build
)
exit /b 0

:need_vs
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo vswhere.exe not found - no Visual Studio or Build Tools on this machine.
    echo Run: scripts\win-dev.bat setup
    exit /b 1
)
rem -prerelease не косметика: установка может быть preview-веткой (VS 18 / MSVC 19.51), и без
rem флага vswhere молча отдаёт пустую строку - «VS не найдена» на машине, где она есть.
set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo Visual Studio is installed, but without the C++ x64/x86 toolchain component.
    echo Run: scripts\win-dev.bat setup
    exit /b 1
)
set "VCVARS=%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo vcvars64.bat missing under "%VSPATH%" - the x64 toolchain is not installed.
    exit /b 1
)
call "%VCVARS%" >nul
if errorlevel 1 (
    echo vcvars64.bat failed.
    exit /b 1
)
echo Toolchain: %VSPATH% ^(x64^)

if /i "%ACTION%"=="shell" (
    echo x64 developer environment is live in this window. Type exit to leave.
    cmd /k
    exit /b 0
)
if /i "%ACTION%"=="probe" goto do_probe
if /i "%ACTION%"=="game" goto do_game
if /i "%ACTION%"=="demo" goto do_demo
if /i "%ACTION%"=="check" goto do_check
if /i "%ACTION%"=="gate8" goto do_gate8
if /i "%ACTION%"=="warn" goto do_warn

:do_build
rem Каталог сборки помнит компилятор: после неудачного захода из 32-битного шелла повторное
rem конфигурирование падает тем же сообщением, пока кеш не удалён. Ловим это до cmake и
rem называем причину, но не удаляем сами - чужой каталог сборки стирается только по команде.
if exist build\CMakeCache.txt (
    findstr /i /c:"Hostx86" build\CMakeCache.txt >nul
    if not errorlevel 1 (
        echo Stale 32-bit cache in build\ from an earlier x86 shell.
        echo Run: scripts\win-dev.bat clean
        exit /b 1
    )
)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build build
if errorlevel 1 exit /b 1
echo.
echo Build OK. Next: scripts\win-dev.bat check   ^(gates^)   or   scripts\win-dev.bat game
exit /b 0

:do_warn
rem Со строгим гейтом сборка встаёт на ПЕРВОМ предупреждении, и каждое следующее стоит отдельного
rem круга «собрал - прислал лог - жди фикс». LIKE_NES_WERROR=OFF существует ровно для этого: пройти
rem дерево целиком и выписать весь список разом. Строгость возвращается тут же, чтобы каталог не
rem остался мягким и не сделал следующий build зелёным по кривой причине.
echo Building with warnings NOT fatal - collecting the full list ...
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLIKE_NES_WERROR=OFF >nul
if errorlevel 1 exit /b 1
cmake --build build > build\warnings.txt 2>&1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLIKE_NES_WERROR=ON >nul
echo.
findstr /r /c:"warning [CD][0-9]" build\warnings.txt
echo.
echo Full log: build\warnings.txt  ^(send this file^)
exit /b 0

:do_probe
rem Гейт 8 спеки #14 — живой пад: цель есть только при INPUT_NATIVE (по умолчанию ON), и здесь
rem досбирается ОДНА она, а не дерево целиком: после pull бинарь протухает молча, а полная сборка
rem ради одной пробы стоит десяток минут. Каталог не конфигурируется — это дело `build`, и он же
rem ловит 32-битный кеш; отсюда только проверка, что конфигурирование уже было.
if not exist build\CMakeCache.txt (
    echo build\ is not configured - run: scripts\win-dev.bat build
    exit /b 1
)
cmake --build build --target framework_input_probe
if errorlevel 1 exit /b 1
rem Путь к манифесту пресетов ОТНОСИТЕЛЬНЫЙ, а рабочий каталог здесь — корень дерева (cd в шапке),
rem поэтому проба не зависит от того, из какой папки её позвали. 2-й аргумент — свой манифест.
set "MANIFEST=%~2"
if "%MANIFEST%"=="" set "MANIFEST=example_ugly_game\assets\input.txt"
if not exist "%MANIFEST%" (
    echo manifest "%MANIFEST%" not found
    exit /b 1
)
rem Вывод этого файла — английский намеренно: консоль Windows не UTF-8, и русский из .bat приезжает
rem кракозябрами (тот же дефект, что был у отчёта owner_check.sh).
echo.
echo Plug the pad in WHILE the probe runs - the CONNECTED line is half of the gate.
build\framework_input_probe.exe "%MANIFEST%"
exit /b %errorlevel%

:do_game
if not exist build\game_sidescroller.exe (
    echo build\game_sidescroller.exe missing - run: scripts\win-dev.bat build
    exit /b 1
)
rem Второй аргумент — ручка выбора адаптера из engine/render/gpu.cpp. На гибридном ноутбуке кадры
rem могут рисоваться на дискретной карте, а окном владеть интегрированная: изнутри процесса всё
rem успешно, на экране чёрное. Здесь только проброс: `game d3d12` меняет бэкенд, `game low` -
rem адаптер на интегрированный. Переменные ставятся строками верхнего уровня, а не внутри
rem if-блока: в блоке %VAR% разворачивается при разборе, то есть до присваивания.
if /i "%~2"=="low" set "LIKENES_GPU_POWER=low"
if /i "%~2"=="high" set "LIKENES_GPU_POWER=high"
if not "%~2"=="" if /i not "%~2"=="low" if /i not "%~2"=="high" set "LIKENES_GPU_BACKEND=%~2"
if not "%~2"=="" echo Adapter: LIKENES_GPU_BACKEND=%LIKENES_GPU_BACKEND% LIKENES_GPU_POWER=%LIKENES_GPU_POWER%
build\game_sidescroller.exe
exit /b %errorlevel%

:do_demo
rem Тот же кадр тем же пайплайном, но в файл вместо окна: окна и поверхности в этом пути нет вовсе.
rem Кадры на месте, а окно чёрное - сломана презентация (драйвер/композиция), а не отрисовка;
rem кадры чёрные - виноват движок, и искать надо в нём.
if not exist build\game_sidescroller.exe (
    echo build\game_sidescroller.exe missing - run: scripts\win-dev.bat build
    exit /b 1
)
if exist build\demo rmdir /s /q build\demo
mkdir build\demo
build\game_sidescroller.exe --demo build\demo --frames 60
if errorlevel 1 exit /b 1
echo.
echo Frames: build\demo\frame_0059.png ^(and 59 more^) - open a late one and look.
exit /b 0

:do_check
set "GATE=scripts/owner_check.sh"
goto run_gate

rem Сквозной гейт клонирует дерево и собирает клон с нуля, то есть ему нужны РАЗОМ vcvars (cl.exe в
rem PATH) и POSIX-шелл - ровно та же пара, что и owner_check.sh, поэтому запуск общий. Из самого
rem Git Bash он не поднимется: тот шелл не видел vcvars64.bat, и клон встанет на поиске компилятора.
:do_gate8
set "GATE=scripts/gate8_e2e.sh"
goto run_gate

:run_gate
rem Гейты писаны на POSIX-шелле, и его на Windows приносит Git. Путь ищется по установкам, а не
rem через where bash: там первым нашёлся бы bash из WSL - другая машина с другой файловой системой.
set "GITBASH="
for %%p in (
    "%ProgramFiles%\Git\bin\bash.exe"
    "%PF86%\Git\bin\bash.exe"
    "%LOCALAPPDATA%\Programs\Git\bin\bash.exe"
) do if not defined GITBASH if exist %%p set "GITBASH=%%~p"
if not defined GITBASH (
    echo Git for Windows not found - it ships the POSIX shell the gates run in.
    echo Run: scripts\win-dev.bat setup
    exit /b 1
)
"%GITBASH%" %GATE%
exit /b %errorlevel%
