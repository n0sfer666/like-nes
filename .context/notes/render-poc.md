# Render-вертикаль PoC — рабочий скретчпад

Спека #2 / ADR 0002 validation gate. Tier T4. Коммит на каждый шаг риска.

## Решения (зафиксированы 2026-07-18)
- Golden ref: **Metal-эталон на этой машине** (offscreen→readback→epsilon). lavapipe/CI — follow-up.
- Структура: **тонкая**, декомпозиция по concern'ам (≤200 строк/файл). Render-graph = простая
  структура + аренный пул транзитных таргетов (доказать инвариант #5). Без backend-agnostic иерархии.
- lighting-technique = один общий WGSL-модуль, потребляемый deferred И forward (доказывает шов).

## G-buffer формат (open Q #1, решено для PoC)
- gbuffer_albedo: rgba8unorm (rgb=albedo, a=emissive-маска/сила)
- gbuffer_normal: rgba8unorm (rgb=нормаль в [0..1], a свободна)
- позиция фрагмента 2D выводится из screen-UV (не нужен position-target)
- HDR-таргет освещения: rgba16float (линейный HDR)
- swapchain/offscreen выход: bgra8unorm / rgba8unorm (после тонмаппинга)

## Инвариант «GPU не кормит сим»
- Sim-домен (позиции спрайтов/светов) в fix32; float ТОЛЬКО на границе рендера (snapshot).
- Нет GPU→CPU→sim readback. Offscreen readback идёт ТОЛЬКО в тест/файл, не в сим.

## Шаги — ВСЕ ЗАКРЫТЫ (T4-гейт зелёный)
1. [x] G-buffer + normal-mapped непрозрачный спрайт (deferred-2D) — commit e641d0e
2. [x] 3 динамических света (deferred lighting → HDR) — commit 344c0a0
3. [x] Forward-пасс прозрачного спрайта (общий shade() → шов technique) — commit e4f31c4
4. [x] Линейный HDR + bloom (render-graph, аренный пул) + ACES тонмаппинг — commit 5be36b3
5. [x] T4 offscreen epsilon/perceptual golden + негативный тест + скриншот — commit e03d590

## Результат
- ADR 0002 → Accepted, спека #2 → Validated. PoC: poc/render/ (render_demo окно, render_golden headless).
- Арена: allocs стабильны на 5 транзитных таргетах (инвариант #5). selftest бит-в-бит run-to-run.
- Известные упрощения / open Q: нормаль не контр-вращается на u.rot (общая 2D-упрощёнка);
  back-to-front сортировки нет (1 прозрачный спрайт); lavapipe-golden для cross-machine CI — follow-up;
  RHI↔Vulkan-шов — open Q #3 (валидируется только 2-м backend'ом).
- naga-грабли: динамический индекс по массиву-значению запрещён → копия в function-var (`var LL=L`, `var w=`).

## Структура файлов (poc/render/)
- gpu.{hpp,cpp} — контекст (headless + surface)
- arena.{hpp,cpp} — пул транзитных таргетов (render-graph resource-manager)
- scene.hpp — sim-состояние (fix32) + snapshot (float на границе)
- sprite.{hpp,cpp} — процедурные albedo+normal текстуры, quad-геометрия
- shaders.{hpp,cpp} — WGSL исходники (raw string), по стадиям
- renderer.{hpp,cpp} — сборка пайплайнов, запись пассов графа, тонмап в выходной view
- capture.{hpp,cpp} — offscreen readback → PNG (stb) + epsilon-сравнение
- demo_main.cpp — оконный цикл (present), опц. --dump PNG
- golden_main.cpp — headless offscreen → epsilon vs golden

## Билд/зависимости
- FetchContent: stb (nothings/stb) для PNG read/write
- Таргеты: render_demo (окно), render_golden (headless тест)
- CI: render_golden headless — ложится в существующий Metal-smoke / Linux lavapipe шаги
