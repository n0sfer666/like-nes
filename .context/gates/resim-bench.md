# Бенч ресима на бюджете зонтика — `brawl_resim_bench`

```
./build/brawl_resim_bench [--against <файл crossplay>]   # CI × 3 ОС, после шага crossplay
```

Спека #25, В9в (2026-10-14). Цель — `engine/framework/ai/brawl_resim_bench.cpp`, драка —
`framework::ai::bench::Fight` в `framework_ai_bench_fight.hpp` (Sim для `rollback::Session`).

Драка: 2 игрока (`scripted`), 8 врагов с мозгом `APPROACH`, 4 ящика (hp 30) и 12 снарядов
(жизнь 24 тика) — ровно `POOL_CAPACITY` 26. Бойцы не умирают (`TOUGH`): мозг снимается только
внутри `step_ai`, и замена павшего в том же тике упёрлась бы в `BRAINS_MAX`. Ящики и снаряды
добираются до бюджета в начале тика (`restock`). Ввод P2 приезжает на 8 тиков позже, а его
`move_x` меняется каждый тик, поэтому каждый из 600 кадров — откат на 8 тиков и переигрывание.

## Что судит

PASS — детерминизм и полнота бюджета, **не время**: p50/p99 и доля кадра 16.6 мс только
печатаются. Цифра раннера описывает раннер; решение о глубине отката и бюджете врагов даёт самый
слабый телефон (В9г, ручной гейт).

- голден драки `resim bench fight hash = 0x0f60b3fbe989d27e` — в шаге CI, в Debug-шаге и в
  `scripts/check_debug_golden.sh` (`STATE_TARGETS`, ci-lint `mirrors-var`);
- ресим через `Session` кончается там же, где прямой прогон;
- `rollbacks == 600`, `replayed == 4800`, `too_deep == 0`;
- минимум по ролям за тик равен бюджету (2/8/4/12);
- `hits_dropped == 0`; контроль — попадания, сломанные ящики и выпущенные снаряды больше нуля;
- `--against`: текст сценариев кроссплея (`имя 0x<16hex>\n`) побайтово равен файлу
  `framework_brawl_crossplay_test --crossplay` той же ОС. Оба пишут/читают в двоичном режиме — CRLF
  на Windows не возникает.

## Позитивный контроль (2026-10-14)

Мутанты `framework_ai_bench_fight.hpp`, каждый падает своей проверкой: ввод P2 постоянный →
«rolled back 8 ticks»; без `restock` → «full umbrella budget» и контроль; `restore` без `ai` или
без `tick` → «resimulated … ends where the straight one does». `--against` с изменённым хешем,
с CRLF и с отсутствующим файлом → FAIL; без пути → код 2.

## Замер

macOS M-серии, Release: снапшот 5904 B, кадр p50 ≈ 43 us (0.25 % кадра), p99 ≈ 49 us; за 600
кадров попаданий 2210, пик 9 из 64 за тик.
