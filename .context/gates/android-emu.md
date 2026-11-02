# Android: два пальца, landscape и вырез (#26 В2д)

```
bash scripts/android_emu_gate.sh
bash scripts/android_emu_gate.sh --required --image 'system-images;android-35;google_apis;x86_64'
python3 scripts/android_touch_protocol_selftest.py
python3 scripts/android_gate_run_selftest.py
python3 scripts/android_gate_avd_selftest.py
```

Гейт доступен на Linux x86_64 и Mac arm64. SDK: Platform Tools, Emulator, Command-line Tools
(avdmanager), rootable API29+ google_apis/default system image под архитектуру хоста. Нет
инструментов — явно SKIP; `--required` превращает это в FAIL. Установленный сломанный инструмент,
краш/таймаут эмулятора и ошибки приложения — FAIL. SDK/NDK/build-tools игры прежние, NDK пин
из android_ndk.sh. preflight вызывает этот гейт; отдельный CI android-emu на Ubuntu/KVM требует
реальный PASS, кэширует system image, cargo/wgpu и task-owned AVD с именованным snapshot.

AVD создаётся только в новом каталоге прогона либо явно заданном `--avd-cache`. Кэш имеет
маркер image/schema/случайного имени AVD и flock; немаркированный непустой каталог не принимается,
несовпадение образа не исправляется удалением. До запуска сверяются canonical `.ini path`,
config identity/image, writable disk paths и отсутствие symlink; path.rel удаляется только из
нового файла собственного AVD, в готовом кэше он запрещён. Указан отдельный console/adb port.
До root проверяются qemu и точное имя AVD, после root/reconnect закрепляется конкретный ADB
transport ID. Перед последующими командами проверяются живой Popen и имя на этом transport;
serial fallback после setup отсутствует. Умерший/заменённый эмулятор не получает install/clear.
В конце завершается только собственный Popen-процесс; подключённые телефоны и чужие AVD не
выбираются. adb server глобально не останавливается. Улики остаются в напечатанном каталоге.

После boot собираются desktop assetc/rumble_assetc и Android SDK, ставятся в пустые префиксы,
настоящий Neon Rumble собирается против них. Запуск интерактивный, extra `probe` — новый nonce.
Вырез задаётся corner-overlay, user_rotation задаётся1/3 при accelerometer_rotation0, затем
эмулятор получает противоположные полюса accelerometer: sensorLandscape предпочитает датчик
блокировке user_rotation ([AOSP DisplayRotation](https://android.googlesource.com/platform/frameworks/base/+/48c42107e624/services/core/java/com/android/server/wm/DisplayRotation.java)).
Фактическая ориентация берётся из `[safe]`; второй жест требует противоположную1/3.
getevent выбирает единственный protocol-B
touchscreen со slot/tracking/position axes. Раскладка читается из controls/touch.txt, точки
переводятся в натуральную портретную ориентацию сырых осей. На каждой стороне стик движется,
затем второй палец берёт a, отпускается a при удержанном стике, отпускается стик.

Судятся реальные `[safe]`/`[touch]` через общий touch_verdict: обе ориентации и смена стороны
выреза, независимые id/владельцы и движение source knob, геометрия и отпускание. Дополнительно
проверяются неизменный PID, точный package/component foreground, два nonblack screencap и фактический `[gpu]` backend
Vulkan + адаптер SwiftShader. GLES и иной адаптер — красные, fallback отсутствует.

Самопроверки без эмулятора различают корректный и9 сломанных транспортов (backend, adapter,
PID, foreground/prefix/title, pixels, protocol B, sensor), проверяют ownership/lock/teardown AVD,
смерть/замену после boot, чужие ini/disk paths, symlinks и границы/raw min
координат. Protocol/controller selftests идут в CI3ОС и preflight, AVD lifecycle — на Unix.
sdk-game дополнительно гоняет реальный MobileRumble CPU через generator sendevent, независимый
декодер protocol B и общий вердикт; GPU/JNI этим не проверяются.18 сломанных реализаций правил
и преобразований проверены при разработке.

Локально 2026-11-02: selftests, CPU pipeline, APK x86_64 против свежих SDK-префиксов PASS;
actionlint, shellcheck, types/lint PASS. Живой новый гейт остановлен на отсутствии avdmanager в
локальном SDK. Ранее установленный AVD падал до приложения (SIGILL init_cache_info/Qt neon).
Linux/KVM CI ещё не запускался: без явного запроса push не выполняется. Это долг проверки,
а не подтверждение работающего Android touch/SwiftShader на устройстве.
