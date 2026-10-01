"""Инвентарь лицензий чужих ассетов `assets/LICENSES.toml` (спека #24, В2): разбор и сверка с
каталогом; титры из него строит `asset_notices.py`.

Пак описан один раз (`[[pack]]`: источник, автор, лицензия, атрибуция), файл ссылается на него
(`[[file]]`: путь, пак, sha256, правки). Двести копий URL и автора у файлов одного пака дали бы пак
с двумя авторами от первой же опечатки.

Каждая находка несёт ВИД: самопроверка требует от сломанной фикстуры отказа именно её вида, а не
какого-нибудь, — иначе фикстура проверяла бы соседнее правило.
"""
import hashlib
import os
import re
import tomllib
from pathlib import Path

from asset_notices import generate
from game_assets import GENERATED, INVENTORY, TEXTS, files_under

ALLOWED = ("CC0-1.0", "CC-BY-3.0", "CC-BY-4.0", "OFL-1.1")
PACK_KEYS = {"name": True, "url": True, "author": True, "license": True, "attribution": False}
FILE_KEYS = {"path": True, "pack": True, "sha256": True, "changes": False}
SHA = re.compile(r"[0-9a-f]{64}")
# Поле едет в `|`-грамматику титров: разделитель, перевод строки или край из пробелов разбор
# `split_fields` прочитал бы иначе, чем написано, а `#` читатели дерева режут как комментарий в
# любом месте строки (atlas_parse, profile_parse, preset_bake).
FIELD = re.compile(r"[^|#\x00-\x1f\x7f]+")


def _attribution_required(spdx):
    return spdx in ALLOWED and spdx.startswith("CC-BY-")


def _table(entry, keys, where, bad):
    if not isinstance(entry, dict):
        bad.append(("schema", f"{where}: not a table"))
        return None
    for key in sorted(set(entry) - set(keys)):
        bad.append(("schema", f"{where}: unknown key '{key}'"))
    for key, required in keys.items():
        value = entry.get(key)
        if value is None:
            if required:
                bad.append(("schema", f"{where}: missing '{key}'"))
        elif not isinstance(value, str) or not FIELD.fullmatch(value) or value != value.strip():
            bad.append(("schema", f"{where}: '{key}' must be one line of text without '|', "
                                  "'#' and edge spaces"))
            return None
    return entry


def _path_ok(path):
    """Печатный ASCII через `/`, без подъёма. Не-ASCII имя macOS отдаёт в NFD, а инвентарь пишется
    в NFC: одно и то же имя расходилось бы со списком каталога только там."""
    parts = path.split("/")
    return (all(" " <= c <= "~" for c in path) and "\\" not in path
            and all(part not in ("", ".", "..") for part in parts))


def parse(text):
    """(паки по имени, файлы по пути, находки)."""
    try:
        data = tomllib.loads(text)
    except tomllib.TOMLDecodeError as e:
        return {}, {}, [("toml", f"{INVENTORY}: {e}")]
    bad = [("schema", f"{INVENTORY}: unknown top-level key '{k}'")
           for k in sorted(set(data) - {"pack", "file"})]
    packs, files = {}, {}
    for kind, keys, into, key in (("pack", PACK_KEYS, packs, "name"),
                                  ("file", FILE_KEYS, files, "path")):
        entries = data.get(kind, [])
        if not isinstance(entries, list):
            bad.append(("schema", f"{INVENTORY}: '{kind}' must be an array of tables [[{kind}]]"))
            continue
        for i, raw in enumerate(entries):
            entry = _table(raw, keys, f"[[{kind}]] #{i + 1}", bad)
            if entry is None or not entry.get(key):
                continue
            if kind == "file" and not _path_ok(entry[key]):
                bad.append(("schema", f"{entry[key]}: path must be printable ASCII relative to "
                                      "assets/, with '/' and without '..'"))
                continue
            if entry[key].lower() in {k.lower() for k in into}:
                bad.append(("schema", f"[[{kind}]] {entry[key]}: listed twice, case ignored "
                                      "(macOS and Windows hold one of two such files)"))
                continue
            into[entry[key]] = entry
    return packs, files, bad


def _rules(packs, files):
    bad = []
    for name, pack in packs.items():
        spdx = pack.get("license")
        if spdx and spdx not in ALLOWED:
            bad.append(("license", f"pack {name}: license {spdx} is not in {', '.join(ALLOWED)}"))
        if spdx and _attribution_required(spdx) and not pack.get("attribution"):
            bad.append(("attribution", f"pack {name}: {spdx} requires 'attribution'"))
        if not any(f.get("pack") == name for f in files.values()):
            bad.append(("schema", f"pack {name}: no [[file]] refers to it"))
    for path, entry in files.items():
        pack = packs.get(entry.get("pack"))
        if entry.get("pack") and pack is None:
            bad.append(("schema", f"{path}: pack '{entry['pack']}' is not declared"))
        elif pack and _attribution_required(pack.get("license", "")) and "changes" not in entry:
            bad.append(("changes", f"{path}: {pack['license']} file needs 'changes' "
                                   "(what was edited, or \"none\")"))
        if entry.get("sha256") and not SHA.fullmatch(entry["sha256"]):
            bad.append(("schema", f"{path}: sha256 must be 64 lowercase hex digits"))
    return bad


def match(listed, inventory):
    """(точные, [(запись, файл)] по регистру, файлы без записи, записи без файла). Каждая запись
    закрывает ровно ОДИН файл: на Linux `a.png` и `A.png` — два файла, и запись `a.png` не должна
    прикрывать второй только потому, что их имена равны без учёта регистра."""
    on_disk = set(listed)
    exact = [p for p in listed if p in inventory]
    loose = {p.lower(): p for p in inventory if p not in on_disk}
    pairs, missing = [], []
    for path in listed:
        if path in inventory:
            continue
        twin = loose.pop(path.lower(), None)
        if twin:
            pairs.append((twin, path))
        else:
            missing.append(path)
    return exact, pairs, missing, sorted(loose.values())


def _listing(base, files):
    listed = files_under(base)
    own = [p for p in listed if p not in (INVENTORY, *GENERATED) and not p.startswith(TEXTS + "/")]
    links = {p for p in own if os.path.islink(base / p)}
    bad = [("link", f"{p}: symbolic link; assets are committed as files") for p in sorted(links)]
    exact, pairs, missing, extra = match(own, files)
    bad += [("case", f"{entry}: directory has {path}") for entry, path in pairs]
    bad += [("missing", f"{p}: not in {INVENTORY}") for p in missing if p not in links]
    bad += [("extra", f"{p}: in {INVENTORY}, no such file") for p in extra]
    for path in exact:
        want = files[path].get("sha256", "")
        if path not in links and SHA.fullmatch(want):
            got = hashlib.sha256((base / path).read_bytes()).hexdigest()
            if got != want:
                bad.append(("sha256", f"{path}: sha256 {got}, inventory says {want}"))
    return bad, [p for p in listed if p.startswith(TEXTS + "/")]


def _texts(packs, texts):
    used = {p["license"] for p in packs.values() if p.get("license") in ALLOWED}
    have = {t for t in texts if t.count("/") == 1 and t.endswith(".txt")}
    bad = [("license-text", f"{t}: only {TEXTS}/<SPDX>.txt of a used license belongs here")
           for t in texts if t not in have or t[len(TEXTS) + 1:-4] not in used]
    bad += [("license-text", f"{TEXTS}/{spdx}.txt: missing for a used license")
            for spdx in sorted(used) if f"{TEXTS}/{spdx}.txt" not in have]
    return bad


def audit(base, write=False):
    """Находки (вид, текст) по каталогу assets/. write — перегенерировать NOTICES и credits."""
    base = Path(base)
    try:
        text = (base / INVENTORY).read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as e:
        return [("toml", f"{INVENTORY}: {e}")]
    if text.startswith("\ufeff"):
        return [("bom", f"{INVENTORY}: starts with a BOM; save it as UTF-8 without BOM")]
    packs, files, bad = parse(text)
    if any(kind == "toml" for kind, _ in bad):
        return bad
    bad += _rules(packs, files)
    listing, texts = _listing(base, files)
    bad += listing + _texts(packs, texts)
    if bad:
        return bad
    for name, data in generate(packs, files).items():
        if write:
            (base / name).write_bytes(data)
        elif not (base / name).is_file() or (base / name).read_bytes() != data:
            bad.append(("notices", f"{name}: stale; run python3 scripts/check_asset_licenses.py "
                                   "--write"))
    return bad
