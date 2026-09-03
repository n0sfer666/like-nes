# Архитектура ядра (высокоуровнево)

> Статус: язык C++ Accepted; архитектура pending PoC. Детали — в спеке #1 и ADR.

## Слои

```
┌──────────────────────────────────────────────┐
│  Gameplay (shared-lib, C-ABI)                 │
│  - системы = чистые функции над ECS-данными   │
│  - fixed-point математика                     │
│  - stateless: не владеет state                │
└───────────────▲──────────────────────────────┘
                │ hot-reload swap (SEH/сигнал-изоляция на границе)
┌───────────────┴──────────────────────────────┐
│  Host / Engine core (C++)                     │
│  - Archetype ECS (flecs) — владеет ВСЕМ state │
│  - детерминированный параллельный планировщик │
│  - fixed timestep loop                        │
│  - арены/пулы + per-frame allocator (ASan/UBSan)│
│  - asset streaming (кастомный бинформат+mmap) │
└───────────────▲──────────────────────────────┘
                │ HAL (Hardware Abstraction Layer)
┌───────────────┴──────────────────────────────┐
│  Backends                                     │
│  - render: Dawn (C++ WebGPU) / wgpu-native    │
│    → Vulkan/Metal/D3D12/GLES                  │
│  - platform: window/input/fs/timer            │
│  - [future] console backend (офиц. C++ SDK)   │
└──────────────────────────────────────────────┘
```

> **HAL ↔ RHI (спека #2):** render-часть HAL — это **RHI** (render-graph-центричный, спека #2).
> Это ОДНА граница, не два слоя: RHI-backend'ы = WebGPU (Dawn/wgpu-native, сейчас) → Vulkan/Metal/
> D3D12/GLES, плюс нативный Vulkan-backend и консольные (GNM/NVN) как отдельные RHI-backend'ы позже.
> Platform-часть HAL (окно/ввод/fs/таймер) — независима от RHI.

## Инварианты (нарушение = баг)

1. **Весь мутабельный игровой state живёт в host, shared-lib — stateless.**
2. **Симуляция детерминирована:** fixed timestep + стабильный порядок систем +
   fixed-point арифметика. Два прогона одного ввода → идентичный хеш состояния.
3. **GPU не кормит симуляцию.** Рендер читает состояние, но не пишет в него.
   GPU-driven сим запрещён (сломал бы детерминизм).
4. **Паника/крэш в gameplay не роняет редактор:** изоляция на границе host↔lib
   (SEH на Windows / signal-хендлеры на *nix), сбойная система отключается.
5. **Аллокации предсказуемы:** горячие пути через арены/пулы/per-frame scratch,
   без скрытых heap-аллокаций в кадре. Safety — вручную (ASan/UBSan в CI, нет borrow-checker).
6. **Платформа входит только через шов `engine/platform/*`** (спека #12, ADR 0012): ввод-вывод
   (`platform_io`, `platform_fs`, `platform_path`), процессы (`platform_process`), модули
   (`platform_module`), изоляция сбоя (`platform_guard`), аргументы запуска в UTF-8
   (`platform_args`), переменные окружения (`platform_env`), UDP-датаграммы (`platform_net`,
   спека #22). Реализацию выбирает CMake парой
   `*_posix.cpp`/`*_win32.cpp`; условной компиляции по ОС вне шва нет — проверяется грепом в CI
   **до** сборки.

## Потоки данных (упрощённо)

```
input → [fixed timestep tick] → ECS state (fixed-point) → render snapshot
                                        │
                                  golden-hash (T4 детерминизм-тест)
```
