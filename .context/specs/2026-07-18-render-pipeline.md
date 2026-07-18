# Спецификация #2: Рендер-пайплайн и спецэффекты (Hollow-Knight-уровень)

- **Дата:** 2026-07-18
- **Статус:** **Validated** (2026-07-18) — дизайн + render-вертикаль PoC (T4) закрыты; ADR 0002 Accepted. См. [ADR 0002](../decisions/2026-07-18-render-pipeline.md). PoC: `poc/render/`.
- **Наследует:** [спека #1](2026-07-18-language-and-core.md) (C++, WebGPU, инвариант «GPU не кормит сим»).

## Контекст

Главный дифференциатор движка — сложная 2D-графика уровня Hollow Knight (нормал-мапленный
свет, пост-процессинг, партиклы, спецэффекты) при приоритете №1 (оптимизация RAM/CPU/GPU).
Цель №1 — Steam-десктоп; мобилки — через quality-tiers. Рендер читает снапшот ECS-состояния,
детерминизма НЕ требует (инвариант спеки #1).

## Требования

### Функциональные
- **Освещение (quality-tiers):** deferred-2D для **непрозрачных** (десктоп) + **forward-пасс
  для прозрачных** (обязателен для HK-визуала) + tiled/clustered forward tier (мобилки), за
  абстракцией «lighting technique»; нормал-мапленные спрайты, динамические источники.
- **Сабмишн:** GPU-driven (indirect draw + culling/сортировка на GPU через compute).
- **Пост-процессинг:** композируемый **render-graph** (bloom, color-grade, дисторшн, …).
- **Партиклы:** GPU (compute-симуляция), визуал-only (не кормят сим, недетерминированы).
- **Цвет:** линейный HDR (десктоп, float targets) / sRGB (мобилки); тонмаппинг.
- **Планы:** layer-система + render targets по слоям (parallax, blend-режимы).
- **Шейдеры:** WGSL напрямую + material-система сверху; hybrid-компиляция (offline для
  релиза, рантайм в редакторе для hot-reload шейдеров); WGSL→SPIR-V (Tint/naga), кеш в
  кастомный ассет-формат (спека #1).
- **Текст:** SDF + растровый fallback (FreeType) для CJK.
- **Разрешение:** виртуальное разрешение + режимы (integer pixel-perfect / smooth HD).

### Нефункциональные (пункт №1)
- CPU-оверхед сабмишна минимизирован GPU-driven путём (research: цена абстракции wgpu ~5-10%).
- Память: per-layer targets и HDR-float — под контролем (мобилки: half-float / sRGB tier).
- **Транзитные GPU-ресурсы render-graph — через пулы/арены, БЕЗ per-frame heap-аллокаций в
  submit-пути** (соблюдение инварианта #5 спеки #1; resource-manager графа переиспользует targets).
- Потолок: WebGPU покрывает ~90% эффектов; топ-GI — через нативный Vulkan-backend (позже).
- **Инвариант «GPU не кормит сим» (спека #1 #3):** результаты GPU-culling/indirect и GPU-партиклы
  НИКОГДА не читаются обратно в ECS/симуляцию (нет GPU→CPU→sim readback).

## Архитектура

```
Game (ECS snapshot) → RenderExtract → RenderGraph (высокоуровневый, backend-agnostic)
                                          │
                        ┌─────────────────┴─────────────────┐
                        │ RHI (render-граница = render-часть │
                        │  HAL спеки #1; render-graph-центр.) │
                        │  backend: WebGPU (сейчас)          │
                        │  backend: native Vulkan (позже)    │  ← escape hatch, топ-эффекты
                        │  backend: console (GNM/NVN, позже) │
                        └────────────────────────────────────┘
RenderGraph-пассы (десктоп-tier) — HYBRID deferred+forward:
  [opaque]      sprite-gbuffer (GPU-driven) → deferred-2D lighting
  [transparent] forward-пасс: та же lighting-technique, sorted back-to-front, alpha-blend
  [particles]   GPU compute → forward-композит (тоже прозрачны)
  → post (bloom/color-grade, линейный HDR) → tonemap → UI/SDF-text → present (virtual-res scale)
```

> **Прозрачность (критично для HK):** deferred хранит 1 слой на пиксель и НЕ тянет alpha-blend.
> Полупрозрачные спрайты (glow, туман, наслоения, партиклы) — через **forward-пасс** с той же
> lighting-technique, отсортированные back-to-front. Материалы компилируются под оба пути.

- **RHI:** render-graph-центричный — движок описывает граф пассов/ресурсов, backend реализует.
- **Фазирование (KISS):** проектируем под ВСЁ (2 backend'а, 2 tier света/цвета), **реализуем
  desktop-first вертикалью на WebGPU**; Vulkan-backend и мобильные tiers вставляются в готовые
  точки расширения позже. (См. риск «scope stacking».)

## API / Интерфейсы

- **Material:** параметры/текстуры + опциональный WGSL-фрагмент; компилируется под активную
  lighting-technique.
- **RenderGraph:** декларация пассов (входы/выходы = логические ресурсы), движок управляет
  render-target'ами/барьерами/переиспользованием.
- **Layer:** parallax-глубина, blend-режим, опциональный собственный target (для эффекта на слое).
- **Camera/Viewport:** виртуальное разрешение + режим масштабирования.

## UI/UX

Авторинг шейдеров/материалов и node-редактор материалов — в IDE-спеке #7 (здесь только
механизм: WGSL+material, hot-reload шейдеров в редакторе). Node-граф материалов — возможный
плагин позже (трётся о «минимализм»).

## Edge Cases

- Мобильный GPU без нужной WebGPU-фичи (compute limits, storage buffers) → feature-detection,
  fallback tier.
- Слишком много динамических светов → cap + приоритизация (deferred масштабируется лучше).
- HDR-float target недоступен/дорог на мобилке → half-float / sRGB tier.
- WGSL→SPIR-V расхождение семантики между backend'ами → golden-image тесты на оба.
- Шейдер не скомпилировался (рантайм-редактор) → показать ошибку, не уронить редактор.
- CJK-глиф вне SDF-атласа → растровый fallback.

## Риски и митигации

| Риск | Вероятность | Митигация |
|------|-------------|-----------|
| **Прозрачность в deferred-2D** (HK = в основном alpha-спрайты; deferred = 1 слой/пиксель) | Высокая | Hybrid deferred+forward: непрозрачные — deferred, прозрачные — forward-пасс той же lighting-technique (в PoC) |
| **Scope stacking** (2 backend'а + 2 tier света/цвета + GPU-driven + render-graph = AAA-масштаб) | Высокая | Фазирование: desktop-WebGPU вертикаль первой; остальное — в точки расширения. НЕ строить 2 backend'а до контента |
| **RHI/technique-швы НЕвалидированы до 2-го backend/tier** → «insert later» = rewrite | Высокая | forward-пасс в PoC валидирует шов lighting-technique; RHI↔Vulkan-шов остаётся open Q #3 (честно, не «готово») |
| Golden-image не воспроизводим байтово между GPU/драйверами/софт-рендерами | Высокая | epsilon/perceptual-сравнение + зафиксированный эталонный рендер (не байтовый хеш) |
| deferred-2D память/бандвидт на слабом железе | Средняя | tiled/clustered forward tier для мобилок; half-float |
| WGSL→SPIR-V непортируемость шейдеров между backend'ами | Средняя | epsilon-регресс на оба backend'а (не байтовый); Tint/naga как единый транслятор |
| GPU-driven сложность (compute culling, indirect) | Средняя | вертикаль-PoC де-рискует; гибрид-fallback на instancing |
| Два backend'а — 2× поддержки | Высокая | реализация Vulkan отложена; RHI-граница спроектирована (но валидируется только 2-м backend'ом) |
| Шейдер-кеш зависит от нерешённого Dawn vs wgpu-native (спека #1 open Q #5) | Средняя | закрыть выбор транслятора (Tint/naga) до финализации шейдер-пайплайна |
| Тест-поверхность растёт от tiers | Средняя | epsilon-golden на tier; CI-матрица |

## Тестовая стратегия

- **Уровень: T4.** DoD = ADR + render-вертикаль PoC + воспроизводимый регресс света/шейдеров/цвета.
- **Render-вертикаль PoC (WebGPU, десктоп):** непрозрачный нормал-мапленный спрайт (deferred-2D)
  **+ полупрозрачный спрайт через forward-пасс** + 2-3 динамических света + линейный HDR + bloom
  (render-graph) + тонмаппинг. Скриншот на экране. **Forward-пасс в PoC заодно валидирует шов
  абстракции lighting-technique** (второй потребитель → «insert later» не превратится в rewrite).
- **T4 регресс — НЕ побайтовый хеш** (GPU-рендер не бит-в-бит между вендорами/драйверами/софт-
  рендерами; golden-hash спеки #1 работал лишь потому, что сим fixed-point). Вместо этого:
  offscreen-рендер → **perceptual/epsilon-сравнение** с эталоном в **зафиксированном окружении
  рендера** (один конкретный софт-рендер, напр. lavapipe, как golden reference). Переносимость
  шейдеров между backend'ами проверяется epsilon-сравнением, НЕ байтовым.
- **Замеры:** GPU-кадр, пик RAM (targets), число draw-вызовов (GPU-driven эффект).
- Offscreen-путь закрывает и render-хвост спеки #1 (headless CI без реального GPU).

## Открытые вопросы

1. Точный формат G-buffer для deferred-2D (albedo/normal/emissive/depth, упаковка каналов) —
   с учётом hybrid deferred+forward (что forward-пассу нужно от G-buffer, если нужно).
2. Число динамических светов в cap'е (десктоп vs мобилка).
3. RHI-граница: точный интерфейс render-graph, чтобы Vulkan/console-backend лёг без переделки
   (валидируется только 2-м backend'ом — риск rewrite).
4. Material-система: где предел параметризации до «нужен свой WGSL»; компиляция под deferred И forward.
5. Интеграция text-shaping (HarfBuzz?) для сложных письменностей — сюда или в UI-спеку #7.
6. Offscreen-рендер для CI (epsilon-golden) — общий с хвостом спеки #1 (headless render).
7. **2D-скелетная анимация** (Spine/sprite-sheet — основа HK-визуала): где живёт (gameplay-side
   или отдельная спека) + как рендер потребляет (GPU vs CPU skinning, нормали при деформации меша).
8. pixel-perfect и bloom/HDR-blur — визуально **взаимоисключающие пресеты** (не одновременно):
   зафиксировать как разные режимы.
9. Соотношение RHI ↔ HAL (спека #1): RHI = render-часть HAL; свести единой диаграммой слоёв.
