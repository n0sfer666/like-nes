"""Данные самопроверки гейта лицензий: исправный каталог и порчи, каждая со своим видом находки."""
import os

INVENTORY = """\
[[pack]]
name = "warped-city"
url = "https://opengameart.org/content/warped-city"
author = "ansimuz"
license = "CC0-1.0"

[[pack]]
name = "neon-signs"
url = "https://karsiori.itch.io/neon-signs"
author = "karsiori"
license = "CC0-1.0"

[[pack]]
name = "fighter-base"
url = "https://opengameart.org/content/base-for-fighting"
author = "CapitanRasputin"
license = "CC-BY-3.0"
attribution = "Base for fighting by CapitanRasputin, CC-BY 3.0"

[[file]]
path = "warped-city/tiles.png"
pack = "warped-city"
sha256 = "{tiles}"

[[file]]
path = "warped-city/tiles.tsj"
pack = "warped-city"
sha256 = "{tsj}"

[[file]]
path = "signs/bar.png"
pack = "neon-signs"
sha256 = "{bar}"

[[file]]
path = "fighter/idle.png"
pack = "fighter-base"
sha256 = "{idle}"
changes = "recoloured to the neon palette"
"""

FILES = {"warped-city/tiles.png": b"\x89PNG\r\n\x1a\ntiles",
         "warped-city/tiles.tsj": b'{\n  "columns": 8,\n  "tilewidth": 16\n}\n',
         "signs/bar.png": b"\x89PNG\r\n\x1a\nbar",
         "fighter/idle.png": b"\x89PNG\r\n\x1a\nidle",
         "licenses/CC0-1.0.txt": b"CC0 1.0 Universal\n",
         "licenses/CC-BY-3.0.txt": b"Attribution 3.0 Unported\n"}


def _edit(base, old, new):
    path = base / "LICENSES.toml"
    text = path.read_text(encoding="utf-8")
    assert old in text, old
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


def put(base, rel, data):
    (base / rel).parent.mkdir(parents=True, exist_ok=True)
    (base / rel).write_bytes(data)


def _reorder(base):
    path = base / "LICENSES.toml"
    blocks = path.read_text(encoding="utf-8").split("\n\n")
    path.write_text("\n\n".join(reversed(blocks)) + "\n", encoding="utf-8", newline="\n")


def _link(rel, target):
    def spoil(base):
        if (base / rel).exists():
            os.remove(base / rel)
        os.symlink(base / target, base / rel)
    return spoil


def _rename_case(base):
    for old, new in (("bar.png", "tmp.png"), ("tmp.png", "Bar.png")):
        os.rename(base / "signs" / old, base / "signs" / new)


# (метка, порча, ожидаемые виды; пусто — находок нет)
CASES = [
    ("нет записи: файл в каталоге без строки инвентаря",
     lambda b: put(b, "signs/pizza.png", b"pizza"), {"missing"}),
    ("лишняя запись: строка инвентаря без файла",
     lambda b: os.remove(b / "signs/bar.png"), {"extra"}),
    ("запрещённая лицензия CC-BY-SA",
     lambda b: _edit(b, 'author = "karsiori"\nlicense = "CC0-1.0"',
                     'author = "karsiori"\nlicense = "CC-BY-SA-4.0"'), {"license"}),
    ("CC-BY без атрибуции",
     lambda b: _edit(b, 'attribution = "Base for fighting by CapitanRasputin, CC-BY 3.0"\n', ""),
     {"attribution"}),
    ("хеш не совпал", lambda b: put(b, "signs/bar.png", b"\x89PNG\r\n\x1a\nbar2"), {"sha256"}),
    ("устаревший NOTICES.txt",
     lambda b: put(b, "NOTICES.txt", (b / "NOTICES.txt").read_bytes() + b"\n"), {"notices"}),
    ("нет текста лицензии", lambda b: os.remove(b / "licenses/CC-BY-3.0.txt"), {"license-text"}),
    ("CRLF-копия файла (autocrlf на Windows)", lambda b: put(
        b, "warped-city/tiles.tsj", FILES["warped-city/tiles.tsj"].replace(b"\n", b"\r\n")),
     {"sha256"}),
    ("устаревший credits.txt", lambda b: os.remove(b / "credits.txt"), {"notices"}),
    ("регистр пути расходится с листингом", _rename_case, {"case"}),
    ("файл CC-BY без поля changes",
     lambda b: _edit(b, 'changes = "recoloured to the neon palette"\n', ""), {"changes"}),
    ("текст неиспользуемой лицензии", lambda b: put(b, "licenses/OFL-1.1.txt", b"OFL\n"),
     {"license-text"}),
    ("неизвестный ключ в записи", lambda b: _edit(b, 'name = "neon-signs"\n',
                                                  'name = "neon-signs"\nlicence = "CC0-1.0"\n'),
     {"schema"}),
    ("`|` в поле, которое едет в титры",
     lambda b: _edit(b, 'author = "ansimuz"', 'author = "ansimuz | Luis"'), {"schema"}),
    ("пробел по краю поля: split_fields его срежет", lambda b: _edit(b, 'author = "ansimuz"',
                                                                     'author = "ansimuz "'),
     {"schema"}),
    ("неизвестная таблица верхнего уровня",
     lambda b: _edit(b, '[[pack]]\nname = "warped-city"',
                     '[meta]\nversion = 1\n\n[[pack]]\nname = "warped-city"'), {"schema"}),
    # Запись с плохим путём отбрасывается разбором: файл остаётся без записи, пак — без файлов.
    ("путь с подъёмом из assets/",
     lambda b: _edit(b, 'path = "signs/bar.png"', 'path = "../signs/bar.png"'),
     {"schema", "missing"}),
    ("не-ASCII путь: macOS отдаёт имя в NFD, инвентарь пишется в NFC",
     lambda b: _edit(b, 'path = "signs/bar.png"', 'path = "signs/b\u00e4r.png"'),
     {"schema", "missing"}),
    ("`#` в поле: читатели `|`-грамматики режут по нему строку",
     lambda b: _edit(b, 'neon-signs"\nauthor', 'neon-signs#red"\nauthor'), {"schema"}),
    ("один путь дважды с разным регистром", lambda b: _edit(
        b, 'changes = "recoloured to the neon palette"\n',
        'changes = "recoloured to the neon palette"\n\n[[file]]\npath = "signs/Bar.png"\n'
        'pack = "neon-signs"\nsha256 = "' + "0" * 64 + '"\n'), {"schema"}),
    ("служебные файлы ОС не находка", lambda b: [put(b, f"signs/{n}", b"x") for n in
                                                  (".DS_Store", "Thumbs.db", "desktop.ini")],
     set()),
    ("пак без файлов", lambda b: _edit(b, '[[file]]\npath = "signs/bar.png"\npack = "neon-signs"',
                                       '[[file]]\npath = "signs/bar.png"\npack = "warped-city"'),
     {"schema"}),
    ("BOM в начале инвентаря", lambda b: put(b, "LICENSES.toml", b"\xef\xbb\xbf"
                                              + (b / "LICENSES.toml").read_bytes()), {"bom"}),
    ("битый TOML", lambda b: _edit(b, '[[pack]]\nname = "warped-city"', '[[pack\nname = "w"'),
     {"toml"}),
    ("ссылка вместо файла", _link("signs/bar.png", "fighter/idle.png"), {"link"}),
    ("ссылка на каталог: обход за неё не идёт", _link("more", "fighter"), {"link"}),
    ("перестановка записей инвентаря не делает генерацию устаревшей", _reorder, set()),
]
