"""Фикстуры гейта лицензий: исправный каталог проходит, каждая порча отказывает своим видом.

Сверяется МНОЖЕСТВО видов находок, а не их наличие: порча, отбитая соседним правилом, доказывала бы
соседнее правило, а своё оставляла бы непроверенным.
"""
import hashlib
import os
import shutil
import tempfile
from pathlib import Path

from asset_licenses import audit, match
from check_asset_licenses_fixtures import CASES, FILES, INVENTORY, put
from game_assets import asset_dirs

# (метка, листинг, инвентарь, ждём (точные, пары по регистру, без записи, без файла)). Пару файлов,
# различающихся только регистром, macOS и Windows на диске не держат — правило проверяется без ФС.
MATCHES = [
    ("a.png и A.png на диске, запись одна: второй файл без записи", ["A.png", "a.png"], {"a.png"},
     (["a.png"], [], ["A.png"], [])),
    ("регистр расходится: пара, а не два отказа", ["A.png"], {"a.png"},
     ([], [("a.png", "A.png")], [], [])),
    ("обе записи, оба файла", ["A.png", "a.png"], {"a.png", "A.png"},
     (["A.png", "a.png"], [], [], [])),
]


def _fixture(base):
    for rel, data in FILES.items():
        put(base, rel, data)
    def sha(rel):
        return hashlib.sha256(FILES[rel]).hexdigest()
    text = INVENTORY.format(tiles=sha("warped-city/tiles.png"), tsj=sha("warped-city/tiles.tsj"),
                            bar=sha("signs/bar.png"), idle=sha("fighter/idle.png"))
    (base / "LICENSES.toml").write_text(text, encoding="utf-8", newline="\n")
    return audit(base, write=True)


def _run(title, ok, verbose):
    if verbose or not ok:
        print(f"  {'ok  ' if ok else 'FAIL'}  {title}")
    return 0 if ok else 1


def selftest(version_refusal, verbose=True):
    failures, total, skipped = 0, 0, 0
    for title, listed, inventory, want in MATCHES:
        failures += _run(f"сопоставление: {title}", match(listed, inventory) == want, verbose)
        total += 1
    with tempfile.TemporaryDirectory() as tmp:
        pristine = Path(tmp, "games/neon-rumble/assets")
        written = _fixture(pristine)
        clean = audit(pristine)
        failures += _run("исправный каталог: --write без находок, затем чисто",
                         not written and not clean, verbose)
        for finding in written + clean:
            print(f"         лишняя находка: {finding}")
        failures += _run("обход видит games/*/assets", asset_dirs(tmp) == [pristine], verbose)
        total += 2
        for title, spoil, want in CASES:
            case = Path(tmp, "case")
            shutil.copytree(pristine, case)
            try:
                spoil(case)
            except (OSError, NotImplementedError) as e:
                # Windows без режима разработчика symlink не создаёт; git там отдаёт ссылку файлом.
                if not title.startswith("ссылка"):
                    raise
                if verbose:
                    print(f"  skip  {title}: {e}")
                skipped += 1
                shutil.rmtree(case)
                continue
            found = audit(case)
            kinds = {k for k, _ in found}
            failures += _run(title, kinds == want, verbose)
            total += 1
            if kinds != want:
                print(f"         ждали {sorted(want)}, получено: {found}")
            shutil.rmtree(case)
    for version, refused in (((3, 10), True), ((3, 11), False), ((4, 0), False)):
        failures += _run(f"версия Python {version}: отказ {refused}",
                         bool(version_refusal(version)) == refused, verbose)
        total += 1
    if verbose or failures:
        print(f"asset-licenses selftest: {'FAIL' if failures else 'PASS'} — "
              f"{total} кейсов, провалов: {failures}, пропущено: {skipped}")
    return 1 if failures else 0
