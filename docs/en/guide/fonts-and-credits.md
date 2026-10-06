# Fonts and credits

English · [Русский](../../ru/guide/fonts-and-credits.md)

The engine draws text with **bitmap fonts**. A glyph is a small grid of pixels that is either on
or off, the bake packs all glyphs into one texture, and text becomes a list of quads like any other
sprite. This page covers the font format, laying out and drawing text, and the credits screen of
Neon Rumble, which is the first place the engine draws text.

## The font file

The manifest bakes a font from a JSON file:

```text
font | monogram | bitmask | assets/monogram/monogram-bitmap.json
```

The file is one object. Each key is **one character**, and its value is the glyph's rows from top
to bottom. Each row is an integer mask in which **bit i is column i**, counted from the left, so
the lowest bit is the leftmost pixel:

```json
{
"1": [0,0,0,4,6,4,4,4,4,31,0,0],
"?": [0,0,0,14,17,16,8,4,0,4,0,0]
}
```

In the `1` above, row 9 is `31`, which is binary `11111`, so the bottom stroke is five pixels wide.
That is the format of [monogram](https://datagoblin.itch.io/monogram) by Vinícius Menézio. Neon
Rumble ships it unchanged.

The bake refuses a file it would draw wrongly:

- every glyph has the same number of rows as the first one, from 1 to 64. That is the line height;
- a row mask is an integer from 0 to 65535, so a glyph is at most 16 px wide;
- a font has 1 to 4096 glyphs, and a key is exactly one character. A control character is refused;
- the font must have a **`?`** glyph, because `?` is drawn for every character the font lacks.
  Without it the bake fails with `has no '?' glyph to draw unknown characters with`.

A glyph's width is the rightmost column it uses. Its **advance**, the distance to the next glyph,
is the width of the widest ASCII glyph plus one pixel. A wider glyph advances by its own width plus
one, so ASCII text is monospaced and a wide letter still does not overlap the next one. A glyph
with no pixels, such as the space, is not drawn and still advances.

The bake packs the glyphs into one RGBA8 texture, 32 cells in a row, and writes the `fonts` table
of the bundle next to it. monogram has 390 glyphs with a line height of 12 px, and its atlas is
224×156 px.

## Laying out and drawing text

Drawing text takes two calls, each on its own buffer:

- `layout_text(font, text, max_width, places)` decodes UTF-8 and places glyphs in font pixels,
  starting at 0,0. `\n` starts a new line. With a non-zero `max_width`, a glyph that would cross
  that width moves to the next line. The wrap breaks between **characters**, not words, so a long
  URL wraps instead of running off the screen. A character the font lacks is drawn as `?` and
  counted in `unknown`.
- `text_quads(font, places, pen, quads)` turns the places into quads. `TextPen` gives the screen
  position, a whole scale and the colour.

Both calls return counts instead of failing. `TextStats` has `placed`, `dropped` (no room left in
`places`), `unknown`, `lines` and `width`. `TextQuadStats` has `quads` and `dropped`. A buffer that
is too small loses glyphs at the end of the text and says how many, so the caller can make it
bigger instead of finding the problem by eye.

## The credits screen

**F1** in the Neon Rumble window opens a screen over the level with the title `Credits · Титры`
and the asset packs of the game, each with its author, license and URL. Each press turns a page,
and the press after the last page closes the screen. The text comes from
the `credits` table, which is baked from `credits.txt` (see [Asset licenses](asset-licenses.md)).
The title is in two languages on purpose: it checks the Latin, the Cyrillic and the middle dot of
the font in one line.

The game opens both tables from the bundle once, when the level loads. It finds the font by name,
checks that the atlas texture has the size the font row expects, and turns every pack into a block
of text:

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
    blocks = compose(table);
    return true;
}
```
<!-- /snippet -->

A page holds as many whole packs as fit the height of the 384×216 play area under the title, so a
pack never breaks between two pages:

<!-- snippet: games/neon-rumble/src/rumble_credits.cpp#credits-pages -->
```cpp
void CreditsQuads::paginate(const Credits& credits, const Frame& f) {
    first_.assign(1, 0);
    const auto most = static_cast<uint32_t>(credits.blocks.size() > 0 ? credits.blocks.size() : 1);
    char text[64];
    title_text(text, most, most);
    const int32_t title = static_cast<int32_t>(layout_text(credits.font, text, f.width, places_).lines) + 1;
    const int32_t rows = f.line_px > 0 ? (f.bottom - f.top) / f.line_px - title : 0;
    int32_t used = 0;
    for (uint32_t i = 0; i < credits.blocks.size(); ++i) {
        const auto lines = static_cast<int32_t>(layout_text(credits.font, credits.blocks[i], f.width, places_).lines);
        if (used > 0 && used + lines > rows) {
            first_.push_back(i);
            used = 0;
        }
        used += lines + 1;
    }
}
```
<!-- /snippet -->

Each frame it lays out the title with the page number `n/N` and then the packs of the page, scaled
the same as the level. Only a pack taller than an empty page can still reach below the bottom
margin; its glyphs there are not drawn and count as dropped:

<!-- snippet: games/neon-rumble/src/rumble_credits.cpp#credits-layout -->
```cpp
void CreditsQuads::draw(const Credits& credits, std::string_view text, const Frame& f, TextPen& pen,
                        Layers& layers, LayerStats& st, uint32_t font_texture, CreditStats& out) {
    const TextStats t = layout_text(credits.font, text, f.width, places_);
    uint32_t kept = 0;
    for (uint32_t i = 0; i < t.placed; ++i)
        if (pen.y + places_[i].y * static_cast<int32_t>(f.scale) + f.line_px <= f.bottom) places_[kept++] = places_[i];
    const TextQuadStats q = text_quads(credits.font, {places_.data(), kept}, pen, quads_);
    layers.append(st, {quads_.data(), q.quads}, font_texture);
    out.lines += t.lines;
    out.glyphs += t.placed;
    out.unknown += t.unknown;
    out.quads += q.quads;
    out.dropped += t.dropped + (t.placed - kept) + q.dropped;
    pen.y += static_cast<int32_t>(t.lines + 1) * f.line_px;
    pen.rgba = BODY_RGBA;
}
```
<!-- /snippet -->

`--headless` lays out every page for a 960×540 window and prints the totals:

```text
neon-rumble: credits screen 960x540 scale 2: 5 pack(s), 2 page(s), 16 line(s), 558 glyph(s), 0 unknown, 560 quad(s), 0 dropped
```

`0 unknown` means every character of the credits has a glyph in the font, and `0 dropped` means
every glyph reached the screen: none was lost to a full buffer or below the bottom margin.
`check_sdk_game.sh` matches this line by a pattern: it requires `5 pack(s)`, `2 page(s)`,
`0 unknown` and `0 dropped`, and leaves the line, glyph and quad counts free, because they move
with any edit of the credits text. A new pack or one more page changes the pattern in the same
commit.
