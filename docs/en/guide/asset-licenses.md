# Asset licenses

English · [Русский](../../ru/guide/asset-licenses.md)

A game ships pictures, fonts and sounds made by other people, and each of them comes with a
license. The engine keeps the record of where every file came from next to the files, checks it on
every commit and shows it to the player in the game's credits. The game's own files, such as the
manifest, the levels and the sheet descriptions, live outside `assets/` and are not listed.

## LICENSES.toml

Every third-party file of a game is under `games/<game>/assets/`, and `assets/LICENSES.toml` lists
all of them. One `[[pack]]` describes a source, and one `[[file]]` describes each file taken
from it:

```toml
[[pack]]
name = "forest-tiles"
url = "https://example.org/forest-tiles"
author = "Jane Doe"
license = "CC-BY-4.0"
attribution = "Forest tiles by Jane Doe, CC BY 4.0"

[[file]]
path = "forest-tiles/tiles.png"
pack = "forest-tiles"
sha256 = "<64 lowercase hex digits of the committed file>"
changes = "recoloured, cut to 16 px tiles"
```

This is the shape of an entry, not a real pack. `games/neon-rumble/assets/LICENSES.toml` holds the
real ones of Neon Rumble.

| Key | Meaning |
|---|---|
| `name` | the pack's name, used by `pack` in its files |
| `url` | where the pack was downloaded |
| `author` | who made it, as the author asks to be credited |
| `license` | an SPDX identifier from the allowed list below |
| `attribution` | the text the license asks to show. Required for CC-BY |
| `path` | relative to `assets/`, with `/` and in the same case as on disk |
| `sha256` | the hash of the file as committed, 64 lowercase hex digits |
| `changes` | what was edited, or `none`. Required for files of a CC-BY pack |

Only four licenses are allowed: **CC0-1.0**, **CC-BY-3.0**, **CC-BY-4.0** and **OFL-1.1**. A
license outside that list is refused, even if it looks permissive. The text of every license in
use is stored as `assets/licenses/<SPDX>.txt`.

## What the check refuses

`python3 scripts/check_asset_licenses.py` runs on every commit. It refuses:

- a file under `assets/` that is not in `LICENSES.toml`, and an entry with no file;
- a file whose hash differs from its entry: `<path>: sha256 <got>, inventory says <want>`. An
  edited picture needs its new hash and, for CC-BY, a new `changes`;
- a pack without a file, a file of an undeclared pack, and the same path listed twice in a
  different case, because macOS and Windows keep only one of two such files;
- a symbolic link, because a link is not the file it points to;
- a missing license text, and a text of a license that no pack uses;
- a `NOTICES.txt` or `credits.txt` that does not match `LICENSES.toml`.

`.gitattributes` marks `games/*/assets/**` as `-text`, so git never changes line endings in an
asset and the hash is the same on every OS. A separate check, `check_asset_budget.py`, limits a
game's `assets/` to 30 MiB.

## Generated files

`python3 scripts/check_asset_licenses.py --write` writes two files from `LICENSES.toml`:

- `NOTICES.txt` lists every pack with its author, source, license and files. Ship it next to the
  game.
- `credits.txt` has one line per pack, for the game to bake:

<!-- snippet: games/neon-rumble/assets/credits.txt -->
```text
# Credits of the third-party assets, one pack per credit line.
# Generated from LICENSES.toml by scripts/check_asset_licenses.py --write; do not edit.
credit | chewbatrij | Chewbatrij | CC0-1.0 | https://opengameart.org/sites/default/files/punchingqueen_gfx.zip
credit | monogram | Vinícius Menézio (@vmenezio) | CC0-1.0 | https://datagoblin.itch.io/monogram
credit | puffolotti-bad-company | Puffolotti | CC0-1.0 | https://opengameart.org/content/bad-company-assorted-military-thugs-universal-prototype-2-for-scrolling-beat-em-up-or-mugen
credit | puffolotti-up2 | Puffolotti | CC0-1.0 | https://opengameart.org/content/universal-prototype-2-for-scrolling-beat-em-up-or-mugen
credit | warped-city | ansimuz | CC0-1.0 | https://opengameart.org/content/warped-city
```
<!-- /snippet -->

Neither file is edited by hand. The check compares them with what `--write` would produce, and a
stale file fails the commit with
`credits.txt: stale; run python3 scripts/check_asset_licenses.py --write`.

## Credits in the game

The manifest bakes `credits.txt` into the `credits` table of the bundle:

```text
credits | credits | assets/credits.txt
```

A line is `credit | pack | author | license | url`, optionally followed by one
`attribution | text` line for the same pack. Lines starting with `#` are comments. An empty field,
a control character and broken UTF-8 are refused when the game is built, not when a player opens
the credits. [Fonts and credits](fonts-and-credits.md) shows how Neon Rumble draws the table on its
**F1** screen.
