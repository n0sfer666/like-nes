"""Титры чужих ассетов из инвентаря лицензий (спека #24, В2): `NOTICES.txt` для людей и
`credits.txt` в `|`-грамматике движка для экрана титров. Оба файла — байт в байт функция инвентаря:
гейт лицензий сверяет их с этой генерацией и перезаписывает по `--write`.
"""
from game_assets import CREDITS, INVENTORY, NOTICES, TEXTS


def generate(packs, files):
    """{имя: байты} для NOTICES.txt и credits.txt. Порядок — по именам: перестановка записей в
    инвентаре не должна давать устаревший файл."""
    stamp = f"Generated from {INVENTORY} by scripts/check_asset_licenses.py --write; do not edit."
    notices = ["Third-party assets of this game and their licences.", stamp, ""]
    credits = ["# Credits of the third-party assets, one pack per credit line.", f"# {stamp}"]
    for name in sorted(packs):
        p = packs[name]
        notices += [f"== {name} ==", f"Author: {p['author']}", f"Source: {p['url']}",
                    f"License: {p['license']} ({TEXTS}/{p['license']}.txt)"]
        credits.append(f"credit | {name} | {p['author']} | {p['license']} | {p['url']}")
        if p.get("attribution"):
            notices.append(f"Attribution: {p['attribution']}")
            credits.append(f"attribution | {p['attribution']}")
        notices.append("Files:")
        for path in sorted(f for f, e in files.items() if e.get("pack") == name):
            changes = files[path].get("changes")
            notices.append(f"  {path}" + (f" (changes: {changes})" if changes else ""))
        notices.append("")
    return {NOTICES: "\n".join(notices).encode("utf-8"),
            CREDITS: ("\n".join(credits) + "\n").encode("utf-8")}
