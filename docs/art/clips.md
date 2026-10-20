# Раскадровка клипов

[← к пакету](README.md) · правила кадра и боксов — [animation.md](animation.md)

Движок крутит клип на 60 Гц, длительность кадра у него в **тиках**: 3 тика — 50 мс, 4 — 67,
5 — 83, 6 — 100, 9 — 150, 12 — 200. У клипа одна длительность на все клетки. Кадр, который держится
дольше, занимает несколько клеток подряд с одним и тем же рисунком.

Колонки таблиц:

- **Рисунки** — какой рисунок стоит в каждой клетке клипа, по порядку. Рисунки нумеруются с 0
  внутри клипа: `0 1 2 3 3 4 4` — пять рисунков в семи клетках, третий и четвёртый держатся вдвое
  дольше.
- **Ключи** — сколько поз из рисунков даёт ChatGPT, остальные — in-betweens.
- **`hit0`** и **События** — номера клеток (с 0 внутри клипа), на которых стоит бокс удара или
  событие.

## Пилот: монах — 60 клеток, 48 рисунков, 24 ключа

| Клип | Рисунки | Тики | Повтор | Ключи | `hit0` | События |
|---|---|---|---|---|---|---|
| `idle` | 0 1 2 3 | 9 | цикл | 2 | — | — |
| `walk` | 0 1 2 3 4 5 | 6 | цикл | 2 | — | `step` в 0 и 3 |
| `jump` | 0 1 1 2 2 2 | 4 | раз | 3 | — | `jump` в 0 |
| `strike1` | 0 1 2 3 4 4 | 3 | раз | 2 | 2 | `swing` в 1 |
| `strike2` | 0 1 2 3 3 4 4 | 3 | раз | 2 | 2 | `swing` в 1 |
| `strike3` | 0 1 2 3 4 4 5 5 6 6 | 3 | раз | 3 | 3–5 | `swing` в 2 |
| `grab` | 0 1 1 | 4 | раз | 1 | — | — |
| `throw` | 0 1 2 3 4 5 5 | 4 | раз | 3 | — | `release` в 3 |
| `hurt` | 0 1 1 | 4 | раз | 1 | — | — |
| `fall` | 0 1 2 | 6 | раз | 2 | — | `land` в 2 |
| `down` | 0 | 12 | цикл | 1 | — | — |
| `getup` | 0 1 2 3 | 6 | раз | 2 | — | — |

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

## Пилот: рядовой 1 — 32 клетки, 29 рисунков, 15 ключей

| Клип | Рисунки | Тики | Повтор | Ключи | `hit0` | События |
|---|---|---|---|---|---|---|
| `idle` | 0 1 2 3 | 9 | цикл | 2 | — | — |
| `walk` | 0 1 2 3 4 5 | 6 | цикл | 2 | — | `step` в 0 и 3 |
| `strike1` | 0 1 2 3 3 4 4 | 3 | раз | 2 | 2 | `swing` в 1 |
| `hurt` | 0 1 1 | 4 | раз | 1 | — | — |
| `fall` | 0 1 2 | 6 | раз | 2 | — | `land` в 2 |
| `down` | 0 | 12 | цикл | 1 | — | — |
| `getup` | 0 1 2 3 | 6 | раз | 2 | — | — |
| `held` | 0 | 12 | цикл | 1 | — | — |
| `thrown` | 0 1 2 | 5 | раз | 2 | — | — |

Ключи рядового: `strike1` — `wind-up: rear fist cocked by the ear, body leaning forward` ·
`straight punch fully extended at face height, rear foot pushing off`; `held` — `grabbed by the
collar, feet barely touching the ground, hands clawing at the grip`; `thrown` — `flung through the
air upside down, limbs flailing` · `crashing shoulder-first into the ground`. Остальные клипы —
те же фазы, что у монаха, с осанкой из [characters.md](characters.md).

## Раскладка листа

Клип лежит в одном ряду листа, поэтому клипы упакованы по рядам по 10 клеток. Кадры в `.pxo`
идут **в этом порядке**: ряд 0 слева направо, затем ряд 1 и так далее. В Pixelorama номер кадра =
ряд × 10 + колонка + 1.

Монах, лист 1920×1152:

| Ряд | Колонки 0–9 |
|---|---|
| 0 | `strike3` 0–9 |
| 1 | `strike2` 0–6, `grab` 7–9 |
| 2 | `throw` 0–6, `hurt` 7–9 |
| 3 | `walk` 0–5, `idle` 6–9 |
| 4 | `jump` 0–5, `getup` 6–9 |
| 5 | `strike1` 0–5, `fall` 6–8, `down` 9 |

```text
image|monk.png
grid|192|192
clip|strike3|row=0|from=0|to=9|ticks=3
clip|strike2|row=1|from=0|to=6|ticks=3
clip|grab|row=1|from=7|to=9|ticks=4
clip|throw|row=2|from=0|to=6|ticks=4
clip|hurt|row=2|from=7|to=9|ticks=4
clip|walk|row=3|from=0|to=5|ticks=6|loop
clip|idle|row=3|from=6|to=9|ticks=9|loop
clip|jump|row=4|from=0|to=5|ticks=4
clip|getup|row=4|from=6|to=9|ticks=6
clip|strike1|row=5|from=0|to=5|ticks=3
clip|fall|row=5|from=6|to=8|ticks=6
clip|down|row=5|from=9|to=9|ticks=12|loop
event|strike3|2|swing
event|strike2|1|swing
event|throw|3|release
event|walk|0|step
event|walk|3|step
event|jump|0|jump
event|strike1|1|swing
event|fall|2|land
```

Рядовой, лист 1920×768:

| Ряд | Колонки 0–9 |
|---|---|
| 0 | `strike1` 0–6, `hurt` 7–9 |
| 1 | `walk` 0–5, `idle` 6–9 |
| 2 | `getup` 0–3, `fall` 4–6, `thrown` 7–9 |
| 3 | `down` 0, `held` 1 |

```text
image|bandit.png
grid|192|192
clip|strike1|row=0|from=0|to=6|ticks=3
clip|hurt|row=0|from=7|to=9|ticks=4
clip|walk|row=1|from=0|to=5|ticks=6|loop
clip|idle|row=1|from=6|to=9|ticks=9|loop
clip|getup|row=2|from=0|to=3|ticks=6
clip|fall|row=2|from=4|to=6|ticks=6
clip|thrown|row=2|from=7|to=9|ticks=5
clip|down|row=3|from=0|to=0|ticks=12|loop
clip|held|row=3|from=1|to=1|ticks=12|loop
event|strike1|1|swing
event|walk|0|step
event|walk|3|step
event|fall|2|land
```

Боксы к этим строкам дописываются руками по [animation.md](animation.md) §Боксы и события.

## Полный набор героя — сверх пилота, 52 кадра

Таблицы ниже ещё в миллисекундах на кадр. В клетки они переводятся так же, как пилот: тик клипа —
самый короткий кадр, каждый кадр занимает целое число клеток, ближайшее к его длительности. Раскладку листа составит
раунд, который их рисует.

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

Полный герой — 100 рисунков, это больше одного листа ([animation.md](animation.md) §Клетка и
пивот). Аура ци супера — отдельный эффект ([props-fauna.md](props-fauna.md)), на кадрах бойца её
нет.

## Враги вне пилота

| Враг | Набор |
|---|---|
| Рядовой 2 | как рядовой 1, `strike1` — прямой удар ногой на уровне груди, `hit0` на кадре 2 |
| Прыгун | рядовой 1 + `run` (6 × 67, цикл), `jump` (3: 50, 133, 200), `jump_kick` (4: 50, 67, 83, 100; `hit0` 1–2) |
| Крупный | `idle`, `walk` (6 × 133), `strike1` (7: 100, 100, 83, 50, 100, 133, 133; `hit0` и `hit1` 3–4), `grab`, `throw`, `hurt`, `fall`, `down`, `getup` (5 × 100). `held` и `thrown` — если раунд крупного решит, что его бросают |
