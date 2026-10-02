"""Каталоги чужих ассетов игр (`games/<игра>/assets/`, спека #24, В2) и их обход.

Общий для гейтов лицензий и бюджета: каталог, который один из них видит, а другой нет, — ровно
та дыра, ради которой обход один. Свои файлы игры (манифест, уровни, листы) живут вне `assets/`.
"""
import os
from pathlib import Path

INVENTORY = "LICENSES.toml"
NOTICES = "NOTICES.txt"
CREDITS = "credits.txt"
TEXTS = "licenses"
GENERATED = (NOTICES, CREDITS)

# Служебные файлы ОС, которые Finder и Проводник кладут в любой открытый каталог. В git их не
# пускает .gitignore, а гейт смотрит рабочее дерево: без этого списка pre-commit падал бы на файле,
# которого нет ни в коммите, ни в игре.
OS_LITTER = frozenset({".DS_Store", "Thumbs.db", "desktop.ini"})


def asset_dirs(root):
    """Каталоги `games/*/assets` в порядке имён игр."""
    games = Path(root, "games")
    if not games.is_dir():
        return []
    return [d / "assets" for d in sorted(games.iterdir()) if (d / "assets").is_dir()]


def files_under(base):
    """Пути всех не-каталогов под base относительно него, через `/`, по порядку. Ссылка на
    каталог тоже попадает сюда: обход за неё не идёт, а решает о ней вызывающий."""
    out = []
    for top, dirs, files in os.walk(base):
        rel = Path(top).relative_to(base)
        for name in sorted(dirs):
            if os.path.islink(os.path.join(top, name)):
                out.append((rel / name).as_posix())
        out += [(rel / name).as_posix() for name in files if name not in OS_LITTER]
    return sorted(out)
