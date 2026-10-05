<!-- en-sha256: 2d5de3e10dd626ca92ef94b6b43502daab65a984713c75efff4f07471e0dd7fb -->

# Шрифты и титры

[English](../../en/guide/fonts-and-credits.md) · Русский

Движок рисует текст **растровыми шрифтами**. Глиф — маленькая сетка пикселей, каждый из которых
либо включён, либо нет; бейк пакует все глифы в одну текстуру, и текст становится списком квадов,
как любой другой спрайт. Эта страница описывает формат шрифта, раскладку и отрисовку текста и
экран титров Neon Rumble — первое место, где движок рисует текст.

## Файл шрифта

Манифест пропекает шрифт из JSON-файла:

```text
font | monogram | bitmask | assets/monogram/monogram-bitmap.json
```

Файл — один объект. Каждый ключ — **один символ**, а значение — ряды глифа сверху вниз. Каждый ряд
— целочисленная маска, в которой **бит i — это колонка i**, считая слева, так что младший бит —
самый левый пиксель:

```json
{
"1": [0,0,0,4,6,4,4,4,4,31,0,0],
"?": [0,0,0,14,17,16,8,4,0,4,0,0]
}
```

У `1` выше ряд 9 равен `31`, в двоичной записи `11111`, так что нижний штрих шириной пять
пикселей. Это формат [monogram](https://datagoblin.itch.io/monogram) Винисиуса Менезиу (Vinícius
Menézio). Neon Rumble поставляет его без изменений.

Бейк отбивает файл, который нарисовал бы неверно:

- у каждого глифа столько же рядов, сколько у первого, от 1 до 64. Это высота строки;
- маска ряда — целое от 0 до 65535, так что глиф не шире 16 px;
- в шрифте от 1 до 4096 глифов, и ключ — ровно один символ. Управляющий символ отбивается;
- у шрифта обязан быть глиф **`?`**: `?` рисуется вместо каждого символа, которого в шрифте нет.
  Без него бейк падает с `has no '?' glyph to draw unknown characters with`.

Ширина глифа — самая правая занятая им колонка. Его **advance**, расстояние до следующего глифа, —
ширина самого широкого ASCII-глифа плюс один пиксель. Более широкий глиф сдвигает перо на свою
ширину плюс один, так что ASCII-текст моноширинный, а широкая буква всё равно не наезжает на
следующую. Глиф без пикселей, например пробел, не рисуется, но перо сдвигает.

Бейк пакует глифы в одну RGBA8-текстуру по 32 клетки в ряд и пишет рядом таблицу `fonts` бандла.
У monogram 390 глифов с высотой строки 12 px, его атлас — 224×156 px.

## Раскладка и отрисовка текста

Отрисовка текста — два вызова, у каждого свой буфер:

- `layout_text(font, text, max_width, places)` декодирует UTF-8 и расставляет глифы в пикселях
  шрифта, начиная с 0,0. `\n` начинает новую строку. При ненулевом `max_width` глиф, который
  пересёк бы эту ширину, уходит на следующую строку. Перенос рвёт между **символами**, а не
  словами, поэтому длинный URL переносится, а не убегает за экран. Символ, которого в шрифте нет,
  рисуется как `?` и считается в `unknown`.
- `text_quads(font, places, pen, quads)` превращает места в квады. `TextPen` задаёт позицию на
  экране, целый масштаб и цвет.

Оба вызова возвращают счётчики вместо отказа. В `TextStats` — `placed`, `dropped` (в `places` не
осталось места), `unknown`, `lines` и `width`. В `TextQuadStats` — `quads` и `dropped`. Слишком
маленький буфер теряет глифы в конце текста и говорит, сколько, так что вызывающий может его
увеличить, а не искать проблему на глаз.

## Экран титров

**F1** в окне Neon Rumble открывает поверх уровня экран с заголовком `Credits · Титры` и тремя
паками ассетов игры, у каждого — автор, лицензия и URL. Текст берётся из таблицы `credits`, которая
пропекается из `credits.txt` (см. [Лицензии ассетов](asset-licenses.md)). Заголовок на двух языках
намеренно: одна строка проверяет латиницу, кириллицу и точку посередине строки в шрифте.

Игра открывает обе таблицы из бандла один раз, при загрузке уровня. Она находит шрифт по имени,
проверяет, что текстура атласа того размера, которого ждёт строка шрифта, и превращает титры в
текст:

<!-- snippet: games/neon-rumble/src/rumble_credits.cpp#credits-open -->
```cpp
bool Credits::open(const Level& level) {
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!level.read_table("fonts", data, size) || !fonts.open(data, size)) {
        std::fprintf(stderr, "neon-rumble: fonts table: does not open\n");
        return false;
    }
    font = fonts.find(Credits::FONT);
    if (font.row == nullptr) {
        std::fprintf(stderr, "neon-rumble: no font %s in the fonts table\n", Credits::FONT);
        return false;
    }
    const asset::LookupFault f = asset::raw_rgba8(level.bundle, font.row->texture_guid, atlas);
    if (f != asset::LookupFault::Ok || atlas.width != font.row->page_w || atlas.height != font.row->page_h) {
        std::fprintf(stderr, "neon-rumble: font atlas %016llx: %s\n",
                     static_cast<unsigned long long>(font.row->texture_guid),
                     f != asset::LookupFault::Ok ? asset::lookup_fault_name(f) : "size differs from the font page");
        return false;
    }
    if (!level.read_table("credits", data, size) || !table.open(data, size)) {
        std::fprintf(stderr, "neon-rumble: credits table: does not open\n");
        return false;
    }
    text = compose(table);
    return true;
}
```
<!-- /snippet -->

Каждый кадр она раскладывает заголовок, а затем тело внутри игрового поля 384×216 в том же
масштабе, что и уровень, и отбрасывает строки, которые упали бы ниже нижнего поля:

<!-- snippet: games/neon-rumble/src/rumble_credits.cpp#credits-layout -->
```cpp
TextPen pen{r.x + margin, r.y + margin, scale, TITLE_RGBA};
for (const char* text : {TITLE, credits.text.c_str()}) {
    const TextStats t = layout_text(credits.font, text, width, places_);
    uint32_t kept = 0;
    for (uint32_t i = 0; i < t.placed; ++i)
        if (pen.y + places_[i].y * static_cast<int32_t>(scale) + line_px <= bottom) places_[kept++] = places_[i];
    const TextQuadStats q = text_quads(credits.font, {places_.data(), kept}, pen, quads_);
    layers.append(st, {quads_.data(), q.quads}, font_texture);
    out.lines += t.lines;
    out.glyphs += t.placed;
    out.unknown += t.unknown;
    out.quads += q.quads;
    out.dropped += t.dropped + (t.placed - kept) + q.dropped;
    pen.y += static_cast<int32_t>(t.lines + 1) * line_px;
    pen.rgba = BODY_RGBA;
}
```
<!-- /snippet -->

`--headless` печатает счётчики той же раскладки для окна 960×540:

```text
neon-rumble: credits screen 960x540 scale 2: 3 pack(s), 12 line(s), 255 glyph(s), 0 unknown, 256 quad(s), 0 dropped
```

`0 unknown` значит, что у каждого символа титров есть глиф в шрифте, а `0 dropped` — что каждый
глиф дошёл до экрана: ни один не потерян ни в полном буфере, ни ниже нижнего поля.
`check_sdk_game.sh` сверяет эту строку по шаблону: он требует `3 pack(s)`, `0 unknown` и
`0 dropped`, а число строк, глифов и квадов оставляет свободным, потому что оно сдвигается от любой
правки текста титров. Четвёртый пак меняет шаблон в том же коммите.
