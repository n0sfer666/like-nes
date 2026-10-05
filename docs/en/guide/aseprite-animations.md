# Animations in Aseprite

English · [Русский](../../ru/guide/aseprite-animations.md)

A character's animations come from an [Aseprite](https://www.aseprite.org/) sprite sheet: a PNG
with the frames and a JSON file that describes them. The manifest needs two lines, one for the
sheet as a texture and one for the clips read from the JSON:

```text
texture | queen_sheet | pixel | assets/chewbatrij/queen-rows.png
clips | queen | aseprite | assets/chewbatrij/queen-rows.json
```

The clips record finds its texture by the `image` field of the JSON, so the texture record must
use the same path. The bake reads the format written by Aseprite 1.3. The sheet in Neon Rumble was
exported by an older Aseprite, and the bake reads it the same way.

## Exporting the sheet

Use **File → Export Sprite Sheet** with these settings:

| Tab | Setting |
|---|---|
| Layout | **By Rows**, **Columns = 8**, no padding |
| Sprite | no trim is needed. A trimmed frame is accepted, and the trim goes into the frame's anchor |
| Borders | **Ignore Empty** and **Merge Duplicates** off |
| Output | **JSON Data**, **Array**, **Tags** and **Slices** ticked. **Split Layers**, **Split Tags** and **Split Slices** off |

The column count matters. With By Rows and an empty column count, Aseprite lays all frames in one
row. The 29 frames of the queen are 74 px wide, so that row is 2146 px, and the bake refuses a
texture side over 2048 px. Eight columns give a 592×300 sheet. Any count that keeps both sides
within 2048 px works.

The same export from the command line, for a build script:

```sh
aseprite -b Queen.ase --sheet queen.png --sheet-type rows --sheet-columns 8 --data queen.json --format json-array --list-tags --list-slices
```

No CI runner has Aseprite, so this command is not run by any gate. It is checked by hand, in step 5
of scenario 19 of the [owner verification](../../owner-verification.md), against the same output
the editor export gives. If a new version of
Aseprite renames a flag, `aseprite --help` is the source of truth.

The bake refuses an export it would read wrongly, and the refusal names the export setting:
`export without --merge-duplicates` for two frames sharing a cell, `export without --ignore-empty`
for a missing frame number, `export without --scale` for a scaled sheet, and a rotated frame.

## Clips

A **clip is a tag**. A frame outside every tag is not part of any clip, and a sheet without tags is
refused with `no tags in 'frameTags'; a clip is a tag, export with --list-tags`.

- **Direction**: `forward`, `reverse`, `pingpong` and `pingpong_reverse` are all supported.
- **Repeat**: a tag without a repeat count loops. Repeat **1** plays the clip once, and a
  ping-pong tag with repeat 1 makes one pass there and back. Any other count is refused, because
  the engine either plays a clip once or loops it.
- **Duration**: each frame's duration in milliseconds is rounded to ticks of the 60 Hz simulation,
  `(ms × 60 + 500) / 1000`. A 100 ms frame is 6 ticks, and 1 to 1 000 000 ms is accepted.

## Hit boxes, hurt boxes and the pivot

Boxes are **slices** with fixed names:

| Slice | Meaning |
|---|---|
| `hit0` … `hit3` | where an attack hits |
| `hurt0` … `hurt3` | where the character can be hit |
| `push` | the body that keeps two characters apart |
| `pivot` | the slice's pivot point is the anchor of the frame, where the character stands |

A slice key applies from its frame until the next key of the same slice. A key with a **0×0** box
turns the box off from that frame on. Keys go in frame order, one key per frame, and a box must lie
inside the frame. A slice with any other name is refused, so a typo in `hit0` is not silently
ignored.

## Events

A tag's **user data** lists events as `<frame>:<event>` pairs separated by commas, with frames
counted from the start of the tag. For example, `3:step,7:swing` fires `step` on the fourth frame
of the clip and `swing` on the eighth. The game receives an event when the clip reaches its frame.

## Without Aseprite

A sheet drawn in another editor can be described by a text file with the `.sheet` extension and
baked with `clips | <name> | sheet | <path>.sheet`. It has one record per line:

```text
image|<path of the sheet PNG>
grid|<cell width>|<cell height>
clip|<name>|row=<r>|from=<a>|to=<b>|ticks=<t>[|loop][|pingpong]
box|<clip>|<frame>|<box>|<x>|<y>|<w>|<h>
event|<clip>|<frame>|<event>
```

`image` and `grid` come before the first clip. Box names are the same as for slices.

## Looking at the result

**F3** in the game window toggles an overlay over the fighter: the frame's cell, the pivot cross
and the boxes of the current frame. `bash scripts/aseprite_owner_check.sh <directory>` bakes a
fresh export from that directory (`queen.json` and `queen.png`) and prints the clip table it read,
so you can see which frames got a box and an event before you open the game.
