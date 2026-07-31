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
rem   scripts\win-dev.bat check   scripts/owner_check.sh шеллом git-bash
rem   scripts\win-dev.bat game    запустить собранную игру
rem   scripts\win-dev.bat shell   отдать cmd с готовым x64-окружением
rem   scripts\win-dev.bat clean   удалить каталог build (кеш помнит компилятор)
setlocal EnableExtensions
cd /d "%~dp0.." || exit /b 1

set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=build"

rem Скобка в самом ИМЕНИ переменной закрывает блок раньше времени, если развернуть её внутри
rem `if (...)` или списка `for`. Разворачиваем один раз здесь, на верхнем уровне.
set "PF86=%ProgramFiles(x86)%"

if /i "%ACTION%"=="setup" goto do_setup
if /i "%ACTION%"=="clean" goto do_clean
if /i "%ACTION%"=="build" goto need_vs
if /i "%ACTION%"=="check" goto need_vs
if /i "%ACTION%"=="game"  goto need_vs
if /i "%ACTION%"=="shell" goto need_vs
echo win-dev.bat: unknown action "%ACTION%"
echo usage: win-dev.bat [setup^|build^|check^|game^|shell^|clean]
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
if /i "%ACTION%"=="game" goto do_game
if /i "%ACTION%"=="check" goto do_check

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

:do_game
if not exist build\game_sidescroller.exe (
    echo build\game_sidescroller.exe missing - run: scripts\win-dev.bat build
    exit /b 1
)
build\game_sidescroller.exe
exit /b %errorlevel%

:do_check
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
"%GITBASH%" scripts/owner_check.sh
exit /b %errorlevel%
