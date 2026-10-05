<!-- en-sha256: d1c56b9497dd6cab4a700cdf68c3d78d9d875ddfa336043bedabf493341dab62 -->

# Лицензии ассетов

[English](../../en/guide/asset-licenses.md) · Русский

Игра поставляет картинки, шрифты и звуки, сделанные другими людьми, и у каждого из них есть
лицензия. Движок держит запись о происхождении каждого файла рядом с файлами, проверяет её на
каждом коммите и показывает игроку в титрах игры. Собственные файлы игры — манифест, уровни,
описания листов — лежат вне `assets/` и не перечисляются.

## LICENSES.toml

Каждый чужой файл игры лежит в `games/<игра>/assets/`, и `assets/LICENSES.toml` перечисляет их
все. Один `[[pack]]` описывает источник, один `[[file]]` — каждый взятый из него файл:

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

Это форма записи, а не настоящий пак. Настоящие паки Neon Rumble лежат в
`games/neon-rumble/assets/LICENSES.toml`.

| Ключ | Значение |
|---|---|
| `name` | имя пака, на которое ссылается `pack` его файлов |
| `url` | откуда пак скачан |
| `author` | кто его сделал — так, как автор просит себя указывать |
| `license` | SPDX-идентификатор из списка разрешённых ниже |
| `attribution` | текст, который лицензия просит показывать. Обязателен для CC-BY |
| `path` | относительно `assets/`, через `/` и в том же регистре, что на диске |
| `sha256` | хеш файла в том виде, в каком он закоммичен, 64 строчные шестнадцатеричные цифры |
| `changes` | что правилось, или `none`. Обязателен для файлов пака CC-BY |

Разрешены только четыре лицензии: **CC0-1.0**, **CC-BY-3.0**, **CC-BY-4.0** и **OFL-1.1**. Лицензия
вне этого списка отбивается, даже если выглядит разрешительной. Текст каждой используемой лицензии
хранится в `assets/licenses/<SPDX>.txt`.

## Что отбивает проверка

`python3 scripts/check_asset_licenses.py` идёт на каждом коммите. Она отбивает:

- файл в `assets/`, которого нет в `LICENSES.toml`, и запись без файла;
- файл, чей хеш расходится с записью: `<path>: sha256 <got>, inventory says <want>`. Правленой
  картинке нужен новый хеш, а для CC-BY — новое `changes`;
- пак без файлов, файл необъявленного пака и один путь, перечисленный дважды в разном регистре:
  macOS и Windows из двух таких файлов оставляют один;
- символическую ссылку: ссылка — не тот файл, на который она указывает;
- отсутствующий текст лицензии и текст лицензии, которую не использует ни один пак;
- `NOTICES.txt` или `credits.txt`, не совпадающий с `LICENSES.toml`.

`.gitattributes` помечает `games/*/assets/**` как `-text`, поэтому git никогда не меняет концы
строк в ассете и хеш одинаков на любой ОС. Отдельная проверка, `check_asset_budget.py`, ограничивает
`assets/` игры 30 МиБ.

## Порождаемые файлы

`python3 scripts/check_asset_licenses.py --write` пишет из `LICENSES.toml` два файла:

- `NOTICES.txt` перечисляет каждый пак с автором, источником, лицензией и файлами. Его кладут рядом
  с игрой.
- `credits.txt` — по строке на пак, для бейка игрой:

<!-- snippet: games/neon-rumble/assets/credits.txt -->
```text
# Credits of the third-party assets, one pack per credit line.
# Generated from LICENSES.toml by scripts/check_asset_licenses.py --write; do not edit.
credit | chewbatrij | Chewbatrij | CC0-1.0 | https://opengameart.org/sites/default/files/punchingqueen_gfx.zip
credit | monogram | Vinícius Menézio (@vmenezio) | CC0-1.0 | https://datagoblin.itch.io/monogram
credit | warped-city | ansimuz | CC0-1.0 | https://opengameart.org/content/warped-city
```
<!-- /snippet -->

Ни один из них не правится руками. Проверка сравнивает их с тем, что произвёл бы `--write`, и
устаревший файл валит коммит с
`credits.txt: stale; run python3 scripts/check_asset_licenses.py --write`.

## Титры в игре

Манифест пропекает `credits.txt` в таблицу `credits` бандла:

```text
credits | credits | assets/credits.txt
```

Строка — `credit | pack | author | license | url`, за ней необязательно одна строка
`attribution | text` для того же пака. Строки, начинающиеся с `#`, — комментарии. Пустое поле,
управляющий символ и битый UTF-8 отбиваются при сборке игры, а не когда игрок открывает титры.
[Шрифты и титры](fonts-and-credits.md) показывает, как Neon Rumble рисует эту таблицу на экране
**F1**.
