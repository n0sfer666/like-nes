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

Промпты монаха и рядового 1 — [prompts.md](prompts.md) §2–3. Model sheet остальных делается ими же:
абзац `Character:` заменяется описанием ниже, рост — числом из таблицы, цвета — рампами персонажа
из [style-bible.md](style-bible.md) §Мастер-палитра.

- **Герои** — промпт §2, но приложить и `monk/model-sheet.png`, а в первый абзац дописать
  `at the same pixel scale as the attached monk model sheet; <рост> pixels against the monk's 96`.
- **Враги** — промпт §3. Подписи строк `swap-cloth` и `swap-sash` («the vest and the trousers»,
  «the sash and the headband») переписываются по колонке «Сменные части» таблицы.

- **Девушка.** `a young woman martial artist in contemporary Korean street-fashion style made of
  fantasy-era materials: oversized cropped jacket with a wrapped hanbok-like collar, high-waisted
  wide pleated trousers, soft ankle boots, high ponytail tied with a long ribbon, two short daggers
  held in reverse grip, light and fast stance`
- **Кикбоксёр.** `a broad-shouldered Russian kickboxer, short crew cut, light stubble, sleeveless
  linen shirt with red and white folk embroidery at the collar and hem, red sash belt, loose trousers
  tucked into soft leather boots, white hand wraps, guard stance with fists up`
- **Рядовой 2.** `a mountain-road bandit kicker, conical straw hat hiding the eyes, long shirt with
  side slits over trousers, wrapped shins, hands held low, a kicking stance`
- **Прыгун.** `a lean bandit acrobat, cloth mask over the lower face, light tight trousers,
  barefoot, crouched springy stance ready to leap`
- **Крупный.** `a huge bandit brute, bald, heavy belly, thick arms with iron bracers, wide sash belt,
  baggy trousers, slow heavy stance with open grabbing hands`
