# Скрипт чистой машины §15/§16 — `clean_check.sh` и `clean_check.ps1`

```
bash clean_check.sh [<архив>.tar.gz]                                  # macOS, Linux: рядом с пакетами
powershell -ExecutionPolicy Bypass -File clean_check.ps1 [<архив>]    # Windows, PowerShell 5.1
make owner-import ARGS=<отчёт>                                        # = bash scripts/owner_import.sh
bash scripts/clean_check_selftest.sh                                  # bash-половина на сломанных пакетах
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/clean_check_selftest.ps1   # CI Windows
```

Ручной гейт правила 18: §15 и §16 судятся только на машине, где дерево не собиралось. Скрипт —
машинная половина, а не замена прохода: окно редактора, Gatekeeper, SmartScreen, ярлык Start,
апгрейд и даунгрейд `.msi` он печатает чек-листом «руками» в конце отчёта.

- **Доставка — копией рядом с пакетом.** Владелец копирует из `scripts/` пару
  `clean_check.sh` + `clean_check_lib.sh` (или `clean_check.ps1` + `clean_check_lib.ps1`) в папку
  с `release/<версия>/`; без второй половины — код 2 с именем недостающего файла. Без аргумента
  скрипт берёт единственный `like-nes-engine-*-<triple>.tar.gz` рядом с собой; два — отказ, а не
  угадывание.
- **Что судит, обе ОС:** `SHA256SUMS` для архива и установщика (CRLF терпит), распаковку дважды —
  деревья равны пофайлово по SHA-256, `like-nes/` в корне, `version.txt` из трёх строк с версией и
  triple имени архива и без `unknown`, 7 непустых лицензий, `assetc --synthetic` с непустым
  бандлом, `editor_shell --gate6` с PNG по сигнатуре и кодом 0 (PNG при ненулевом коде — FAIL).
- **macOS:** `.dmg` — attach, ярлык `Applications` на томе, `--gate6` из `like-nes.app`, detach.
  Карантин `com.apple.quarantine` у архива или `.dmg` — `note` до проверок (отказ ниже может быть
  словом Gatekeeper). **Linux:** `.AppImage --gate6`; без FUSE — повтор с
  `APPIMAGE_EXTRACT_AND_RUN=1`. Узнаётся по `libfuse|FUSE|fusermount|/dev/fuse`: статический
  type2-runtime appimagetool пишет `Cannot mount AppImage, please check your FUSE setup`, старый —
  `libfuse.so.2`; иной отказ загрузчика — без повтора.
- **Windows (§16):** `where vcruntime140.dll` — найден → `note` и финал `PASS, but §16 stays open`
  (ящик гейт не отвечает); 4 файла в `like-nes\bin`; `msiexec /i /qn /l*v` — код 0, `1603` с
  `Error 1925` в логе назван как InstallScope; 4 файла в `%LOCALAPPDATA%\like-nes\bin`, `--gate6`
  установленного, `msiexec /x /qn` и каталог исчез. Уже стоящий `%LOCALAPPDATA%\like-nes` — FAIL
  до установки: иначе прогон судил бы чужую установку. Коды процесса: `0xC0000135` назван
  «DLL is missing», прочие отрицательные — hex.
- **Отчёт** `clean-check-<os>-<UTCвремя>.txt` рядом с пакетами, кадры `…-tar.png`, `…-dmg.png` /
  `…-appimage.png` / `…-msi.png` рядом. Домашний каталог маскируется `~` построчно при записи
  (оборванный прогон дома не оставляет); граница — любой символ вне `[A-Za-z0-9_-]` или конец
  строки, так что `/Users/alx` при доме `/Users/al` не трогается, а `/Users/al,` — да. Windows —
  ещё короткое 8.3-имя, без учёта регистра, с `\` и с `/`. Папка без права записи — код 2 до
  проверок.
- **`make owner-import ARGS=<отчёт>`** — на машине разработки: имя `clean-check-?*`, в отчёте нет
  `/Users/`, `/home/`, `C:\Users\` → копия `report.txt` и кадров того же префикса в
  `build-owner-run/import/<имя>` и публикация `owner_run_publish` в `refs/owner-runs/main`. Каталоги
  CI-раннеров (`/Users/runner`, `/home/runner`, `C:\Users\runneradmin`) из паники собранного там
  кода — не дом владельца, пропускаются. Список домов — подстановкой, а не `grep -q` в конвейере:
  под `pipefail` ранний выход `-q` даёт первому grep SIGPIPE, и `if` молча пропустил бы отчёт.

## Самопроверка

`clean_check_selftest.sh` (+ сквозной прогон `clean_check_e2e.sh`) под `/bin/bash`: 51 случай на macOS, 47 на Linux, 37 в git-bash Windows
(сквозной прогон и карантин там не идут). Суммы ×4, распаковка ×3 (подменённый `tar` дописывает
файл во вторую), версия ×5, лицензии ×3, assetc ×3, gate6 ×6 (PNG при крахе — FAIL; процесс идёт в каталоге лога — `imgui.ini` не остаётся рядом с пакетом, находка T3), AppImage ×4
(откат на extract-and-run по обеим формулировкам и отказ без повтора), маска ×3 (знак препинания
после дома), карантин ×2, `CC_LICENSES` против `cmake/licenses.manifest` и `$CcBin` против
`expected_files` ветки MINGW (копии без сверки разошлись бы молча), owner-import ×4 (пути
раннеров), сквозной прогон на поддельном пакете ОС раннера (`.dmg` через `hdiutil` /
`.AppImage`-скрипт): PASS, маска, отчёт есть и без дома, кадры архива и установщика, `--gate6`
установщика, его сумма, нет установщика, два архива, папка без записи (не под root); на macOS —
том без ярлыка и том без `.app`. В CI × 3 ОС и в preflight. Мутанты: 15 первого прохода и 11 после
ревью (FUSE, `--gate6` из `.dmg`, условие тома, сумма установщика, граница маски, маска в
`cc_step`, отчёт не записан, карантин, число лицензий, состав bin, пути раннеров) — все убиты.

`clean_check_selftest.ps1` + `clean_check_e2e.ps1` — 53 случая, только CI Windows (`pwsh` и
`powershell` локально нет): exe-фикстура собирается `Add-Type -OutputType ConsoleApplication`,
`msiexec` и `where` подменены функциями `Invoke-CcMsi`/`Get-CcWhere`, `tar` — переменной
`$CcTar`. Путь с пробелом доходит до процесса, процесс идёт в каталоге лога, код и stderr возвращаются, отсутствующая программа —
`-1`; сквозной прогон с подменёнными `USERPROFILE`/`LOCALAPPDATA`/`TEMP`: PASS, маска, установленный
bin, сумма `.msi`, удаление, `-msi.png`, `PASS, but` при найденном Redistributable, 1603 + Error 1925, прежняя
установка, нет `.msi`, пустая папка — код 2, скрипт без lib — код 2. Файлы `.ps1` — только ASCII:
PowerShell 5.1 читает файл без BOM в ANSI, `§` собирается `[char]0xA7`.

## Находки

- Проверка `.ps1` возможна только прогоном CI: первая версия ни разу не запускалась до push.
