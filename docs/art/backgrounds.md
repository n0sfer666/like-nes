# Фоны и тайлы

[← к пакету](README.md)

## Этап

Один этап — горная дорога к монастырю, четыре части слева направо:

| Часть | Ближний план | Дорога | Передний план |
|---|---|---|---|
| Деревня — **пилот** | дома с черепичными крышами, заборы, развешанное бельё | утоптанная земля, камни, мостовая у колодца | столб с фонарём, кусты |
| Бамбуковая роща | плотные стволы бамбука, просветы солнца | тропа с корнями и опавшими листьями | стволы бамбука поперёк кадра |
| Верёвочный мост | пропасть, скальные стены, канаты | доски моста, канатные поручни | свисающие канаты, птицы |
| Ворота храма | каменная лестница, стены, ворота с черепичным навесом | каменные плиты | каменные фонари |

## Планы

Шесть слоёв: 4 задних, игровая дорога и 1 передний. «Параллакс» — доля скорости камеры, с которой
движется слой. Стартовые значения задаёт уровень Tiled, здесь — ориентир для рисунка.

| Слой | Что | Размер после сетки | Параллакс | Хромакей |
|---|---|---|---|---|
| 0 небо | градиент рассвета ступенями, солнце низко слева, облака | 640×360 | 0 | нет, слой непрозрачный |
| 1 дальние горы | хребты в тумане, светлые и холодные | 640 × ~200, бесшовный по x | ~0.1 | `#ff00ff` над хребтом |
| 2 склоны | склоны с соснами, туман у подножия | 640 × ~180, бесшовный по x | ~0.3 | да |
| 3 ближний | по части этапа (таблица выше) | 640 × ~220, бесшовный по x | ~0.6 | да |
| 4 дорога | тайлмап 16×16, по нему ходят бойцы | по уровню | 1 | — |
| 5 передний | редкие предметы по части этапа | 640 × до 360 | ~1.2 | да |

- **Слои тише бойцов.** Задние планы не берут рампы `ink` и белый, контраст падает с расстоянием
  ([style-bible.md](style-bible.md) §Признаки стиля).
- **Передний план почти пуст.** Он закрывает не больше 15 % ширины экрана, и ни один предмет не
  выше половины роста бойца в зоне, где идёт бой.
- **Бесшовность по x.** Движок повторяет слой по горизонтали, поэтому правый край слоя
  продолжает левый ([export.md](export.md) §Бесшовный слой).

## Промпт плана

**Референс:** `style-ref.png` и уже принятые планы этой части этапа, чтобы держались свет и
линия горизонта.

```text
Pixel art background layer for a side-scrolling game, in exactly the style of the attached
style reference. Wide landscape image. <LAYER>. Only this layer: everything that would be in
front of it or behind it is flat solid magenta #FF00FF. No characters, no text, no frame.
Seen straight from the side, horizon at the same height as in the attached layers.
Dawn light from the low sun on the left, warm ochre and gold on lit faces, dusty blue shadows,
mist between depths. Crisp pixels, stepped shading, no gradients except stepped sky bands,
no blur. Use only these colours: <hex рамп слоя>.
```

`<LAYER>` для пилота, деревня:

- **0:** `the sky at dawn: stepped horizontal colour bands from deep dusty blue at the top to peach
  and pale gold near the horizon, a low sun on the left edge, a few long thin clouds`
- **1:** `far mountain ridges fading into mist, pale cold blue-grey, snow on the highest peaks`
- **2:** `mid-distance mountain slopes covered with dark pine trees, mist pooling at their foot`
- **3:** `a small mountain village: two-storey wooden houses with curved grey tiled roofs, a low
  stone wall, a wooden fence, laundry hanging on a line, all seen from the road side`
- **5:** `foreground objects only: one wooden post with a paper lantern on the left, a low bush
  on the right, the rest empty`

## Приёмка плана

- [ ] на изображении только свой слой, всё остальное — ровный `#ff00ff`;
- [ ] горизонт и свет совпадают с принятыми слоями той же части;
- [ ] у слоёв 1–3 правый край продолжает левый после склейки ([export.md](export.md));
- [ ] слой дальше — светлее и холоднее слоя ближе;
- [ ] нет людей, животных и текста (живность — отдельные спрайты, [props-fauna.md](props-fauna.md)).

## Тайлы дороги и моста

Тайл 16×16. ChatGPT плохо держит повторяющийся узор, поэтому он даёт **образец материала**, а
тайлы режутся и сшиваются в Pixelorama.

```text
Pixel art texture swatch, top-down-ish side view of <MATERIAL>, flat even lighting from the left,
same style and colours as the attached reference, square image, no objects, no text.
```

`<MATERIAL>`: `packed dirt road with small stones` (**пилот**) · `worn stone pavement with moss in
the cracks` · `wooden bridge planks with rope bindings` · `large temple stone slabs`.

1. Образец уменьшается до сетки ([export.md](export.md) §Сетка) и квантизуется.
2. Из него вырезаются тайлы 16×16: заливка, верхний край (переход к ближнему плану), нижний
   край, два-три варианта заливки, чтобы узор не повторялся.
3. Каждый тайл проверяется в **View → Tiled Mode → Both Axes**: стык не виден.

**Приёмка тайлсета:** все тайлы 16×16 без зазоров на листе, заливка бесшовна по обеим осям, края
стыкуются с заливкой, тона только из палитры.
