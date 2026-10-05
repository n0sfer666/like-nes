# Levels in Tiled

English · [Русский](../../ru/guide/tiled-levels.md)

A level is a [Tiled](https://www.mapeditor.org/) map saved as JSON (`.tmj`), with its tilesets saved
next to it as `.tsj`. The bake reads the files Tiled writes. It does not need an export step or a
plugin, and saving the map in Tiled is enough for the next build to pick it up. This page describes
what the bake accepts, using `games/neon-rumble/levels/level1.tmj` as the example. Every refusal
named below is printed by the build and contains the name of the layer, tile or object it is
about.

## The map

The bake reads Tiled **1.10 or newer**. `level1.tmj` is saved by Tiled 1.12. The map must be:

- **orthogonal** with **square tiles** of 1 to 1024 px. Isometric and hexagonal maps are refused.
  Neon Rumble uses a 40×21 map of 16 px tiles;
- **finite**: `the map is infinite; turn Map Properties -> Infinite off`;
- saved with **Tile Layer Format = CSV**. Base64 and zlib layers are refused with the same hint;
- with **Parallax Origin 0,0**. The origin moves every parallax layer at once, and the engine
  measures parallax from the top-left corner of the map.

`backgroundcolor` (Map Properties → Background Color, `#RRGGBB` or `#AARRGGBB`) is the colour of
everything the layers do not cover. That includes the bands around the play area in a window with a
different aspect ratio, so it should be set even if the layers cover the whole view. Neon Rumble
uses `#0b0f1a`.

## Tilesets

A tileset is built from **one image** (New Tileset → Based on Tileset Image) and saved as a `.tsj`
file the map points to. The bake refuses a "collection of images" tileset, non-square tiles, a tile
size different from the map's, a transparent colour (put transparency into the PNG alpha), a
drawing offset, and an image whose size on disk differs from what the tileset says. The last case
means the PNG changed after the tileset was made: reload the image in Tiled.

**Animated tiles** use Tiled's own animation editor. Frame durations are converted to ticks of the
fixed-step simulation, so a frame of 150 ms is 9 ticks at 60 Hz. A frame that is itself an animated
tile is refused, because Tiled draws such a frame static and the engine would not.
`neon-signs.tsj` holds the two signs of the level: the neon banner with four frames of 150 ms and
the Coca-Cola sign with three frames of 200 ms.

## Layers

| Layer | What the bake does with it |
|---|---|
| Tile layer | drawn in map order. Flipped and rotated tiles are drawn as Tiled shows them |
| Image layer | one PNG drawn at the layer's offset. It needs a `texture` record with the same path in the manifest |
| Object layer | its objects go into the `objects` table; it is not drawn |
| Group | its children are baked as if they were at the top level: offsets add up, parallax and opacity multiply |

A layer with `visible: false` (the eye icon is off) is not baked at all. Opacity is kept, and
**Tint Color** is refused: tint the PNG instead.

### Parallax

**Parallax Factor** (`parallaxx`, `parallaxy`) is how fast a layer moves relative to the camera:
1 moves with the world, 0.5 at half the speed, 0 does not move at all. `level1.tmj` has three image
layers behind the street:

| Layer | Parallax | Repeat | Property |
|---|---|---|---|
| `sky` | 0 | x | — |
| `far-city` | 0.25 | x | `cover_y = false` |
| `near-city` | 0.5 | x | `cover_y = false` |

A layer slower than the camera shows its edges sooner than the map does, so it usually repeats.
An image layer repeats with **Repeat X / Repeat Y** (`repeatx`, `repeaty`). A tile layer repeats
with the boolean custom properties `repeat_x` and `repeat_y`, because Tiled has no repeat setting
for tile layers.

The collision layer must keep parallax 1. A layer whose offset or parallax adds up past ±32767
across its groups is refused.

### Proving that the layers cover the view

The game draws a **384×216** play area scaled by a whole factor. A window with another aspect ratio
shows more of the level along one axis, up to **576×312** pixels of the world. What lies beyond
that is filled with `backgroundcolor`. A layer that moves slower than the camera can run out of
image within that area, and nobody would notice until a player opens the game in a 21:9 window.

The level record can make the bake check this:

```text
level | level1 | tiled | levels/level1.tmj | viewport
```

With `|viewport`, the bake moves the camera over every position inside the `bounds` rectangle of
the level, or over the whole map if there is none. For each position it checks that every layer
covers the widest possible view, with its parallax, its offset and up to 8 px of camera shake. A
layer that repeats on an axis always covers that axis. There are two rules:

- a layer with parallax below 1 on x must repeat on x. Otherwise the bake fails with
  `map 'level1': layer 'far-city' has parallax x below 1 and must repeat on x`;
- the layer must cover the view on both axes. Otherwise the bake fails with
  `map 'level1': layer 'far-city' does not cover the view: x short by N px left, M px right; y short by …`,
  which says by how much the layer falls short.

A skyline is often meant to end above the street, with the background colour or another layer
below it. The boolean property **`cover_y = false`** on such a layer turns the y check off for that
layer, and the x check stays. `far-city` and `near-city` in `level1.tmj` use it.

## Collision

Exactly one tile layer has the boolean property **`collision = true`**. Its tiles do not draw unless
the layer also has **`visible_too = true`**. In Neon Rumble, the street is both the floor and the
picture. A tile used on the collision layer must have the string property `flags` in its tileset,
made of words separated by spaces:

| Word | Meaning |
|---|---|
| `solid` | blocks from every side |
| `oneway` | holds only from above. `solid oneway` is a platform you can jump through |
| `slope_br`, `slope_bl`, `slope_tr`, `slope_tl` | a 45° slope, with its right angle in the named corner |
| `ladder` | can be climbed and blocks nothing |

A tile without `flags` on the collision layer is refused, and so is a slope flipped vertically or
rotated. A slope can only be flipped horizontally.

## Objects

An object's **Class** and its name go into the `objects` table together with its shape and custom
properties. Tiled writes the class as `type`, and Tiled 1.9 wrote it as `class`. The bake reads
either, and refuses an object that has both with different values. Allowed shapes are a point, a
rectangle and a polygon. Allowed property types are `string`, `int`, `float` and `bool`. An ellipse,
a polyline, a text object, a tile object, a rotated object and an object from a template are
refused, and each refusal says what to do instead.

The bake gives meaning to one class only: **`bounds`**, the rectangle the camera stays within,
which the cover check above walks. The rest is for the game to interpret. `level1.tmj` has three objects in the `spawns` layer:

| Class | Name | Shape | What Neon Rumble does with it |
|---|---|---|---|
| `spawn` | `player` | point at 200, 224, property `facing = right` | places the fighter |
| `bounds` | `street` | rectangle 104, 56, 432×224 | limits the camera |
| `depth_band` | `walk` | rectangle 0, 192, 640×32 | the strip the fighter walks along |

## Checking a level after editing it

`bash scripts/check_sdk_game.sh --keep` bakes the game against the installed SDK and prints the
summary of the level, for example
`level level1 40x21 tile 16, 6 visual layer(s), 16 animated tile(s), 5 texture(s) …`. It also
compares the bundle hash with `games/neon-rumble/bundle.hash`. Saving the map in Tiled without
changing anything must keep the same hash. A different hash means the bake depends on how the file
is formatted, which is a bug in the bake and not in the map.
