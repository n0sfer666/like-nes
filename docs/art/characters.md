# Персонажи: model sheets

[← к пакету](README.md)

Model sheet — эталон персонажа, по которому принимается каждый следующий кадр. Рост ниже — **после
сетки**, от подошвы до макушки в стойке, в пикселях игры ([export.md](export.md) §Сетка). Разница в
росте задумана: персонажи узнаются силуэтом.

| Персонаж | Рост, px | Силуэт в одну строку | Сменные части |
|---|---|---|---|
| Монах — **пилот** | 96 | сухой старик, чуть сгорблен, широкие рукава, борода до груди | нет |
| Девушка с кинжалами | 102 | узкая, высокий хвост, укороченная куртка, широкие брюки | нет |
| Кикбоксёр | 114 | плечистый, короткая стрижка, бинты на кистях | нет |
| Рядовой 1 (кулачник) — **пилот** | 104 | стёганый жилет, повязка на лбу, кулаки | `swap-cloth` жилет и штаны, `swap-sash` пояс и повязка |
| Рядовой 2 (ногами) | 104 | коническая соломенная шляпа, длинная рубаха с разрезами | `swap-cloth` рубаха, `swap-sash` обмотки голеней |
| Прыгун | 100 | гибкий, маска на пол-лица, лёгкие штаны, босой | `swap-cloth` штаны, `swap-sash` маска |
| Крупный | 135 | массивный, лысый, живот, железные наручи | `swap-cloth` штаны, `swap-sash` пояс |

Кинжалы девушки — часть её стиля боя, а не предмет: на земле их нет, подобрать их нельзя.

## Описания для промпта

Описание вставляется в шаблон вместо `<DESCRIPTION>`.

- **Монах.** `an old wiry martial-arts monk, slightly hunched, bald head, long white beard reaching
  the chest and long white eyebrows, saffron robe with very wide sleeves tied at the waist with a
  rope belt, a necklace of large dark wooden prayer beads, cloth wraps on forearms and shins, straw
  sandals, calm sharp eyes; fights with open palms and low kicks`
- **Девушка.** `a young woman martial artist in contemporary Korean street-fashion style made of
  fantasy-era materials: oversized cropped jacket with a wrapped hanbok-like collar, high-waisted
  wide pleated trousers, soft ankle boots, high ponytail tied with a long ribbon, two short daggers
  held in reverse grip, light and fast stance`
- **Кикбоксёр.** `a broad-shouldered Russian kickboxer, short crew cut, light stubble, sleeveless
  linen shirt with red and white folk embroidery at the collar and hem, red sash belt, loose trousers
  tucked into soft leather boots, white hand wraps, guard stance with fists up`
- **Рядовой 1.** `a mountain-road bandit, lean build, quilted padded vest over bare arms, cloth
  headband, sash belt, loose trousers with wrapped shins, bare fists, aggressive forward lean`
- **Рядовой 2.** `a mountain-road bandit kicker, conical straw hat hiding the eyes, long shirt with
  side slits over trousers, wrapped shins, hands held low, a kicking stance`
- **Прыгун.** `a lean bandit acrobat, cloth mask over the lower face, light tight trousers,
  barefoot, crouched springy stance ready to leap`
- **Крупный.** `a huge bandit brute, bald, heavy belly, thick arms with iron bracers, wide sash belt,
  baggy trousers, slow heavy stance with open grabbing hands`

## Шаблон промпта model sheet

**Референс:** `style-ref.png` ([style-bible.md](style-bible.md)). Для второго и следующих
персонажей — ещё один принятый model sheet, чтобы держался общий масштаб.

```text
Pixel art character model sheet for a side-scrolling beat 'em up, in exactly the style of the
attached style reference. Flat solid magenta background #FF00FF, no text, no labels, no frames.
Character: <DESCRIPTION>.
Show the same character four times in one row, all standing on the same horizontal ground line,
same scale, feet on the line: (1) neutral standing, full profile facing right; (2) fighting
stance, full profile facing right; (3) three-quarter front view; (4) back view.
The figure is drawn as a game sprite about <HEIGHT> pixels tall, 1-pixel outline, stepped cel
shading, warm rim light from the left, cool blue shadows. No cast shadow on the ground.
Use only these colours: <hex рамп персонажа из style-bible.md>.
```

Для врага с `swap-*` к промпту добавляется строка с ролью рамп:

```text
The vest and trousers use only these four blues: #1e2340 #33406e #4f67a3 #7f9fd6.
The sash and headband use only these four reds: #3a1420 #6b2232 #a83a44 #e0705a.
No other part of the character uses these colours.
```

## Приёмка model sheet

- [ ] все четыре фигуры — один и тот же персонаж: лицо, причёска, одежда, аксессуары совпадают;
- [ ] подошвы на одной линии, масштаб фигур одинаков;
- [ ] после сетки рост в стойке равен значению из таблицы ±2 px;
- [ ] силуэт отличается от силуэтов уже принятых персонажей;
- [ ] у врага сменные части лежат целиком в своих рампах, и эти тона больше нигде не встречаются;
- [ ] контур 1 px, свет слева, фон — ровный `#ff00ff` без полутонов у края фигуры;
- [ ] нет текста, подписей и тени на земле.

## Дрейф

- *Фигуры в ряду разного роста.* Повторить с `all four figures exactly the same height`. Если
  не помогло, принять лучшую фигуру в профиль и запросить остальные ракурсы отдельно, приложив её.
- *Сменные цвета протекли на кожу или металл.* Не перегенерировать, исправить при квантизации:
  [export.md](export.md) §Рампы переносит пиксели в нужную рампу по маске части.
- *Монах потерял бороду, чётки или рукава.* Это самый трудный персонаж пилота. Назвать
  потерянную деталь первой фразой описания и приложить последний кадр, где она была.
- *Девушка «уходит» в современный город (джинсы, кроссовки).* Добавить `fantasy-era fabrics,
  no modern items, no logos`.
