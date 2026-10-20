# Пропсы, эффекты и живность

[← к пакету](README.md)

Пропсы рисуются тем же порядком, что и бойцы: промпт, сетка и палитра ([export.md](export.md)),
лист и `.sheet` в Pixelorama ([workflow.md](workflow.md)). Клетка меньше, чем у бойца: 48×48 у
предметов и живности, 192×192 у эффекта ауры. Пивот — низ-центр клетки, предмет стоит на её
нижнем ряду.

| Предмет | Размер, px | Клипы | Где |
|---|---|---|---|
| Ящик | 32×28 | `idle`, `break` (4 кадра: трещина, разлёт досок) | везде, из него выпадает еда или метательное |
| Камень | 10×8 | `idle`, `spin` (4) | метательное |
| Горшок | 14×16 | `idle`, `spin` (4), `break` (3) | метательное, бьётся о цель |
| Кунай | 16×4 | `idle`, `spin` (4) | метательное |
| Еда | 16×12 | `idle` | лечение: миска риса, булочки на пару, жареная рыба на палочке |
| Аура ци | по фигуре бойца | `charge` (4, цикл), `burst` (4, раз) | эффект супера поверх бойца |
| Искра удара | 24×24 | `spark` (3, раз) | на точке попадания |

## Промпт пропсов

**Референс:** `style-ref.png`.

```text
Pixel art game props in exactly the style of the attached style reference, flat solid magenta
background #FF00FF, no text. Draw in one row, side view, lit from the left, each object on the
same ground line: <OBJECTS>. Crisp pixels, 1-pixel outline, stepped shading, no ground shadow.
Use only these colours: <hex рамп earth, gold, hair, ink>.
```

`<OBJECTS>`: `a wooden crate bound with rope; a round grey stone; a clay pot with a narrow neck; a
steel throwing knife with a ring pommel; a bowl of white rice; three steamed buns on a leaf; a
grilled fish on a stick`.

Кадры разлёта и вращения не генерируются: вращение — поворот исходного кадра на 90° в Pixelorama,
разлёт — собранные из частей обломки.

**Приёмка:** размеры из таблицы ±2 px, предмет читается на фоне дороги, еда отличается от
метательного силуэтом и цветом (тёплая и светлая против серого и тёмного).

## Аура и искры

Аура не генерируется: она рисуется руками, рампой `gold`, вокруг силуэта бойца. Контур фигуры
снимается с кадра `idle` (**Select → Color Range** по прозрачности, **Modify → Expand** на 2–3 px) и
заливается полупрозрачными языками, которые в `charge` поднимаются вверх, а в `burst` разлетаются
от центра. Искра удара — четыре луча рампы `gold` и белая середина.

## Живность

Косметика вне симуляции: она читает, где идёт драка, и не влияет на неё.

| Существо | Размер, px | Клипы | Поведение |
|---|---|---|---|
| Птица (воробей или сорока) | 12×10 | `perch` (2, цикл), `takeoff` (3), `fly` (4, цикл) | сидит на ветке или крыше, взлетает, когда драка рядом |
| Ящерица | 16×6 | `idle` (2, цикл), `run` (4, цикл) | греется на камне, убегает за край камня |
| Зверь в кустах (лиса или енотовидная собака) | 24×18 | `peek` (3), `hide` (3) | выглядывает из куста, прячется при драке |

```text
Pixel art small animals for a side-scrolling game, same style as the attached reference, flat
solid magenta background #FF00FF, no text. In one row, side view facing right, tiny game sprites:
<ANIMAL AND POSES>. Crisp pixels, stepped shading, no ground shadow.
```

`<ANIMAL AND POSES>`: `a sparrow perched, the same sparrow with wings raised taking off, the same
sparrow in flight with wings down` · `a small grey lizard basking on a stone, the same lizard
running` · `a fox head peeking out of a bush, the same bush with the fox hidden`.

**Приёмка:** силуэт читается на своём размере, существо не похоже на врага или предмет (ни одного
тона `swap-*`, мельче любого пропса).
