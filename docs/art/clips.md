# Раскадровка клипов

[← к пакету](README.md) · правила кадра и боксов — [animation.md](animation.md)

Имя клипа — имя тега в Aseprite. Длительности даны в миллисекундах на кадр. Бейк округляет их до
тиков 60 Гц: 50 мс — 3 тика, 67 — 4, 83 — 5, 100 — 6, 133 — 8, 150 — 9, 200 — 12. Повтор «цикл» —
тег без числа повторов, «раз» — повтор 1. «Ключи» — сколько поз даёт ChatGPT, остальные кадры —
in-betweens и повторы. Кадры `hit0` и события считаются от 0 внутри тега.

## Пилот: монах — 48 кадров, 24 ключа

| Клип | Кадры | мс | Повтор | Ключи | `hit0` | События |
|---|---|---|---|---|---|---|
| `idle` | 4 | 150 | цикл | 2 | — | — |
| `walk` | 6 | 100 | цикл | 2 | — | `0:step,3:step` |
| `jump` | 3 | 50, 133, 200 | раз | 3 | — | `0:jump` |
| `strike1` | 5 | 50, 50, 50, 67, 83 | раз | 2 | 2 | `1:swing` |
| `strike2` | 5 | 50, 50, 50, 83, 100 | раз | 2 | 2 | `1:swing` |
| `strike3` | 7 | 67, 50, 50, 50, 100, 100, 100 | раз | 3 | 3–4 | `2:swing` |
| `grab` | 2 | 67, 133 | раз | 1 | — | — |
| `throw` | 6 | 67, 67, 67, 83, 100, 133 | раз | 3 | — | `3:release` |
| `hurt` | 2 | 67, 133 | раз | 1 | — | — |
| `fall` | 3 | 100 | раз | 2 | — | `2:land` |
| `down` | 1 | 200 | цикл | 1 | — | — |
| `getup` | 4 | 100 | раз | 2 | — | — |

Позы-ключи монаха для промпта (`<POSE n>` в [animation.md](animation.md)):

| Клип | Ключи |
|---|---|
| `idle` | `relaxed hunched stance, hands loosely in front of the belly` · `same stance, chest slightly raised, beard lifted by breath` |
| `walk` | `contact: front heel down, rear toes down, arms swinging opposite` · `passing: one leg straight under the body, the other knee raised, body at its highest` |
| `jump` | `crouch before the jump, knees deeply bent, arms back` · `rising, legs extended down, arms up, sleeves trailing` · `tucked in the air, knees to chest, sleeves fluttering` |
| `strike1` | `wind-up: weight back, front palm pulled to the shoulder` · `front arm fully extended at shoulder height with an open palm, rear hand at the chin` |
| `strike2` | `wind-up: torso turned away, rear palm cocked at the hip` · `rear arm fully extended with an open palm, torso turned toward the viewer` |
| `strike3` | `chamber: front knee raised to the chest, arms out for balance` · `low side kick fully extended at knee height, torso leaning back` · `recovery: kicking foot set down, palms in guard` |
| `grab` | `reaching forward with both hands at chest height, gripping a collar` |
| `throw` | `gripping with both hands, hips loaded, knees bent` · `twisting the torso, pulling over the hip` · `follow-through, arms extended forward and down, weight on the front foot` |
| `hurt` | `head snapped back, torso bent back, arms flung out, eyes shut` |
| `fall` | `knocked off the feet, body horizontal in the air, limbs trailing` · `back hitting the ground, beard and sleeves still in the air` |
| `down` | `lying on the back on the ground line, beads spread on the chest` |
| `getup` | `rolling to one knee, one palm on the ground` · `rising from the knee, hands coming up to guard` |

## Пилот: рядовой 1 — 29 кадров, 15 ключей

| Клип | Кадры | мс | Повтор | Ключи | `hit0` | События |
|---|---|---|---|---|---|---|
| `idle` | 4 | 150 | цикл | 2 | — | — |
| `walk` | 6 | 100 | цикл | 2 | — | `0:step,3:step` |
| `strike1` | 5 | 67, 67, 50, 83, 100 | раз | 2 | 2 | `1:swing` |
| `hurt` | 2 | 67, 133 | раз | 1 | — | — |
| `fall` | 3 | 100 | раз | 2 | — | `2:land` |
| `down` | 1 | 200 | цикл | 1 | — | — |
| `getup` | 4 | 100 | раз | 2 | — | — |
| `held` | 1 | 200 | цикл | 1 | — | — |
| `thrown` | 3 | 83 | раз | 2 | — | — |

Ключи рядового: `strike1` — `wind-up: rear fist cocked by the ear, body leaning forward` ·
`straight punch fully extended at face height, rear foot pushing off`; `held` — `grabbed by the
collar, feet barely touching the ground, hands clawing at the grip`; `thrown` — `flung through the
air upside down, limbs flailing` · `crashing shoulder-first into the ground`. Остальные клипы —
те же фазы, что у монаха, с осанкой из [characters.md](characters.md).

## Полный набор героя — сверх пилота, 52 кадра

У всех трёх героев одни и те же клипы и длительности, различается только рисунок: ладони и низкие
удары ногами у монаха, кинжалы обратным хватом у девушки, руки и лоу-кики у кикбоксёра.

| Клип | Кадры | мс | Повтор | Ключи | `hit0` | События |
|---|---|---|---|---|---|---|
| `run` | 6 | 67 | цикл | 2 | — | `0:step,3:step` |
| `block` | 2 | 67, 200 | раз | 1 | — | — |
| `dodge` | 4 | 67 | раз | 2 | — | — |
| `super` | 10 | 83 × 5, 50 × 3, 133 × 2 | раз | 4 | 5–7 (+ `hit1`) | `0:charge,5:burst` |
| `dash_strike` | 5 | 50, 50, 67, 83, 100 | раз | 2 | 2–3 | `1:swing` |
| `stomp` | 6 | 67, 67, 50, 83, 100, 100 | раз | 3 | 3 | `3:impact` |
| `climb` | 4 | 100 | цикл | 2 | — | — |
| `climb_strike` | 4 | 67, 50, 83, 100 | раз | 2 | 2 | `1:swing` |
| `item_pick` | 3 | 67, 100, 67 | раз | 2 | — | `1:pick` |
| `item_throw` | 4 | 67, 67, 83, 100 | раз | 2 | — | `2:release` |
| `held` | 1 | 200 | цикл | 1 | — | — |
| `thrown` | 3 | 83 | раз | 2 | — | — |

Полный герой — 100 кадров, это два листа ([animation.md](animation.md) §Ячейка и пивот). Аура ци
супера — отдельный эффект ([props-fauna.md](props-fauna.md)), на кадрах бойца её нет.

## Враги вне пилота

| Враг | Набор |
|---|---|
| Рядовой 2 | как рядовой 1, `strike1` — прямой удар ногой на уровне груди, `hit0` на кадре 2 |
| Прыгун | рядовой 1 + `run` (6 × 67, цикл), `jump` (3: 50, 133, 200), `jump_kick` (4: 50, 67, 83, 100; `hit0` 1–2) |
| Крупный | `idle`, `walk` (6 × 133), `strike1` (7: 100, 100, 83, 50, 100, 133, 133; `hit0` и `hit1` 3–4), `grab`, `throw`, `hurt`, `fall`, `down`, `getup` (5 × 100). `held` и `thrown` — если раунд крупного решит, что его бросают |
