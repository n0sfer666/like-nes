# Мобильный экран и тач #26 В2г

Из корня, после сборки:

```
cmake --build build --target rumble_touch_test rumble_touch_quads_test rumble_overlay_gpu_test
./build/rumble_touch_test
./build/rumble_touch_quads_test
./build/rumble_overlay_gpu_test
bash scripts/check_sdk_game.sh
```

- `rumble_touch_test` — настоящие controls/touch.txt, TouchSource, ActionMap, Lobby и command_of.
  Пять кнопок, DZ.3/порог.75/K3, Lost до Free, один экран на место, safe-only cancel. Тест в
  Release/Debug/ASan CI; шесть production-мутантов проверены локально при реализации.
- `rumble_touch_quads_test` — пять кругов внутри hit-областей,22 глифа подписей, плавающий стик,
  held/cancel, точки×плотность1/2.75/3, видимость при любом новом паде/касании/владении местом.
  Без GPU, в Release/Debug/ASan CI.
- `rumble_overlay_gpu_test` — реальные пиксели через QuadRenderer/submit_quads/readback. Красная
  сцена ограничена показанным viewport, зелёный UI — поверх полос. В каждом прогоне также два
  неправильных clip-фикстуры: узкий UI и сцена на весь экран обязаны отличаться. Отдельный
  обязательный шаг CI трёх ОС, без continue-on-error. Два production clip-мутанта проверены
  локально, оба отвергнуты.
- `sdk_game_mobile_cpu.sh` внутри sdk-game — настоящий MobileRumble и общие sources.cmake,
  engine из desktop prefix, переносимый mobile::Game из дерева, мок только mobile_host.
  Поверхность не подключается, GPU не инициализируется. Семь сценариев: demo480=desktop,
  idle/move/cancel с разными sim-hash, rotate=suspend=cancel, поздний вход Joining61/Present62. Проверяются реальные
  pointer/scale/cancel и NaN до fix32. Скрипт поверх тача, потерянный pointer, demo off-by-one
  и пропуск позднего входа при сохранении ownership локальными production-мутантами не проходят. Строгая сборка; скрипт использует native-пути
  на Windows. UIKit/JNI и GPU этим не проверяются.
- Пакеты installed SDK дополнительно проверяют compiled Android theme/manifest и iOS plist;
  JNI mock26 исключений и сломанный NewStringUTF — в [sdk-game](sdk-game.md).

Результат 2026-11-02: CPU Release/Debug и полный инструментированный ASan+UBSan граф PASS;
GPU PASS в локальном Linux-контейнере на Vulkan/llvmpipe LLVM19, GCC14.2,6 production cpp + test
и обычная pinned wgpu-native библиотека. Это не полный Linux game/CI и не аппаратный GPU.
На Mac native WebGPU не находит адаптер, CoreSimulatorService не отвечает, Android AVD падает
до приложения (SIGILL init_cache_info, GUI Qt processor/neon), в том числе вне sandbox.
Ни живой тач, ни повороты на телефоне не засчитаны; долг — `~/wiki/dev-log/like-nes/checks/26.md`.
В2д: `python3 scripts/touch_verdict.py --selftest` проверяет общий вердикт без платформенных
зависимостей (CI трёх ОС, preflight). Восемь корректных историй,29 сломанных,6 неверных раскладок
и2 CLI-политики:
нонс, обе landscape-ориентации1/3 и смена выреза, раздельные safe-эпохи, реальные Down/Up и
владельцы, движение ручки источника и независимое отпускание огня в одной lifetime-паре,
геометрия из touch.txt. iOS допускает симметричную ненулевую safe area UIKit; Android cutout
overlay обязан менять сторону. Шесть сломанных реализаций правил отвергнуты самопроверкой.
Совпадение id32 информационное; общий удерживаемый native id — ошибка. iOS-обёртка вызывает
тот же модуль с раскладкой шутера; XCTest читает её оригинал как ресурс и снимает4 кадра.
Полноценный Android-сценарий двух пальцев — следующий шаг В2д.
