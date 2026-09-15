"""Фикстуры шва хешей: каждое правило обязано сработать на поломке и промолчать на починке.

Тот же контракт, что у `line_budget_selftest.py`, и по той же причине: гейт, чьё правило сломано,
молчит неотличимо от гейта, которому нечего сказать. Правила проверяются синтетическим словарём
«путь → текст», охранники — своим, а обход — НАСТОЯЩИМ репозиторием: он берёт файлы у git, и
подмена механизма означала бы, что набор проверяет не то, чем гейт пользуется на дереве.

Ключ поиска — подстрока, которую обязана назвать находка. Сравнивать с полным текстом значило бы
пинить формулировку, и первая же правка сообщения красила бы самопроверку, ничего не проверив.
"""
import subprocess
import tempfile
from pathlib import Path

from hash_seam_rules import PRIMITIVES

# Примитивы стоят в КАЖДОЙ фикстуре: без них охранники валили бы опорные кейсы по чужой причине.
BASE = {PRIMITIVES[0]: "constexpr uint64_t FNV_PRIME = 1099511628211ull;\nh *= FNV_PRIME;\n",
        PRIMITIVES[1]: "constexpr uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;\nh *= FNV_PRIME;\n"}

PRIME = "прайм FNV-1a-64"
CANON = "каноническая база"
ASSET = "семьи asset"
FORM = "смешивание FNV руками"
ALLOW3 = "// hash-seam: allow причина ровно из трёх"

# (метка, ключ в находке, сломанный файл, починенный файл)
CASES = [
    ("прайм шестнадцатеричным", PRIME, "h *= 0x100000001b3ull;\n", "h *= mul;\n"),
    ("тот же прайм десятичным", PRIME, "h *= 1099511628211ull;\n", "h *= mul;\n"),
    ("каноническая база", CANON,
     "uint64_t h = 0xcbf29ce484222325ull;\n", "uint64_t h = physics::seed();\n"),
    ("база семьи asset", ASSET,
     "uint64_t h = 1469598103934665603ull;\n", "uint64_t h = asset::seed();\n"),
    # Написания в дереве сегодня нет вовсе, и это ровно тот случай, ради которого сравнение идёт по
    # ЗНАЧЕНИЮ: гейт на подстроке пропустил бы его молча.
    ("канонический offset десятичным", CANON,
     "uint64_t h = 14695981039346656037ull;\n", "uint64_t h = physics::seed();\n"),
    ("верхний регистр и суффикс", PRIME, "h *= 0X100000001B3ULL;\n", "h *= mul;\n"),
    ("разделители разрядов", PRIME, "h *= 1'099'511'628'211;\n", "h *= mul;\n"),
    ("маркер-отписка не подавляет", PRIME,
     "h *= 1099511628211ull; // hash-seam: allow надо\n",
     "h *= 1099511628211ull; %s\n" % ALLOW3),
    ("маркер через строку не накрывает", PRIME,
     "%s\n\nh *= 1099511628211ull;\n" % ALLOW3, "%s\nh *= 1099511628211ull;\n" % ALLOW3),
    # Освобождены ДВА названных пути, а не «файл, в имени которого есть hash»: под такое правило
    # подпадает первая же новая копия, названная похоже.
    ("файл с похожим именем не освобождён", CANON,
     "uint64_t h = 0xcbf29ce484222325ull;\n", "uint64_t h = physics::seed();\n"),
    # Правило формы: копия пишется ИМЕНОВАННЫМИ константами, и литеральное правило её не видит.
    ("именованный прайм в умножении", FORM,
     "h ^= b;\nh *= asset::FNV_PRIME;\n", "h = asset::fnv1a_u32(h, v);\n"),
    ("именованная константа целым словом", FORM,
     "h = (h ^ v) * asset::FNV_PRIME;\n", "h = asset::fnv1a_word(h, v);\n"),
    ("умножение перед xor", FORM,
     "g = g * framework::physics::FNV_PRIME ^ s;\n", "g = mixed(g, s);\n"),
    ("неквалифицированное имя", FORM, "h *= FNV_PRIME;\n", "h = fnv1a_u64(h, v);\n"),
    ("маркер-отписка не подавляет форму", FORM,
     "h *= asset::FNV_PRIME; // hash-seam: allow надо\n",
     "h *= asset::FNV_PRIME; %s\n" % ALLOW3),
    # Литералы гасятся ДО поиска комментариев: `"http://x"` иначе открывал бы комментарий до конца
    # строки и прятал бы код, стоящий за ним.
    ("литерал со слэшами не открывает комментарий", PRIME,
     'const char* u = "http://x"; h *= 0x100000001b3ull;\n',
     'const char* u = "http://x"; h *= mul;\n'),
    ("маркер внутри литерала не подавляет", PRIME,
     'const char* s = "%s";\nh *= 1099511628211ull;\n' % ALLOW3,
     "%s\nh *= 1099511628211ull;\n" % ALLOW3),
]

# Чистые входы: находок быть не должно ни одной. Гейт, который ругается на честное дерево, снимают
# целиком — и вместе с ним уходит всё, что он ловил.
QUIET = [
    ("дерево без констант вне примитивов", "uint64_t h = physics::seed();\n"),
    ("константа в строчном комментарии",
     "// база 0xcbf29ce484222325 против нашей 1469598103934665603\n"),
    ("константа в блочном комментарии", "/* прайм 0x100000001b3 общий у обеих семей */\n"),
    ("маркер на той же строке",
     "h *= 1099511628211ull; // hash-seam: allow смешивание mtime с размером\n"),
    ("маркер на предыдущей строке",
     "// hash-seam: allow смешивание mtime с размером\nh *= 1099511628211ull;\n"),
    ("соседнее число не из списка", "h *= 0x100000001b4ull;\n"),
    # Затравка арифметикой не является: отбивать её значило бы запретить звать примитив.
    ("затравка именованной константой", "uint64_t h = asset::FNV_OFFSET;\n"),
    ("аргумент по умолчанию", "uint64_t f(const void* p, size_t n, uint64_t h = FNV_OFFSET);\n"),
    ("константа в строковом литерале", 'const char* s = "0x100000001b3";\n'),
]

BAD_PATH = "engine/other/copy.cpp"
LOOKALIKE = "engine/asset/hash2.hpp"

FULL = dict(BASE, **{f"engine/f{i}.cpp": "int x = 1;\n" for i in range(70)})
NO_NUM = {p: "constexpr uint64_t P = mul;\nh *= FNV_PRIME;\n" for p in PRIMITIVES}
NO_FORM = {p: "constexpr uint64_t FNV_PRIME = 1099511628211ull;\nh *= P;\n" for p in PRIMITIVES}

# (метка, ключ в отказе, дерево)
GUARDS = [
    ("примитив не найден", "не нашёл примитив", dict(FULL, **{PRIMITIVES[0]: None})),
    ("обход мимо дерева", "при пороге", BASE),
    ("разбор чисел сломан", "разбор чисел сломан", dict(FULL, **NO_NUM)),
    ("правило формы сломано", "правило формы сломано", dict(FULL, **NO_FORM)),
]


def _fixture_repo(root):
    """Настоящий репозиторий: обход берёт файлы у git, а не у os.walk."""
    for rel, text in (("engine/a.cpp", "int a;\n"), ("engineering/b.cpp", "int b;\n"),
                      ("docs/examples/c.cpp", "int c;\n"), ("engine/notes.md", "текст\n"),
                      ("engine/skip.cpp", "int s;\n")):
        (root / rel).parent.mkdir(parents=True, exist_ok=True)
        (root / rel).write_text(text, encoding="utf-8")
    (root / ".gitignore").write_text("engine/skip.cpp\n", encoding="utf-8")
    subprocess.run(["git", "init", "-q"], cwd=root, check=True)
    subprocess.run(["git", "add", "-A"], cwd=root, check=True)
    # Ненаписанный в индекс файл берётся наравне с индексом: иначе свежая копия падала бы только
    # на коммит-гейте, когда под неё уже написан код.
    (root / "engine/new.cpp").write_text("int n;\n", encoding="utf-8")


def _run(check, title, want, verbose):
    status = "PASS" if want else "FAIL"
    if verbose or not want:
        print(f"  [{status}] {title}")
    return 0 if want else 1


def selftest(audit, guards, scan, verbose=True):
    failures = 0
    for title, key, bad, ok in CASES:
        path = LOOKALIKE if "похожим именем" in title else BAD_PATH
        fired = [f for f in audit({**BASE, path: bad}) if key in f]
        silent = [f for f in audit({**BASE, path: ok}) if key in f]
        failures += _run(audit, title, bool(fired) and not silent, verbose)
        if not fired:
            print("         правило промолчало на сломанной фикстуре")
        for finding in silent:
            print(f"         правило сработало на починенной фикстуре: {finding}")
    for title, text in QUIET:
        found = audit({**BASE, BAD_PATH: text})
        failures += _run(audit, f"no-false-positive: {title}", not found, verbose)
        for finding in found:
            print(f"         лишняя находка: {finding}")
    # Опорный кейс охранников идёт ПЕРВЫМ: не пройди его исправное дерево — все четыре порчи ниже
    # отбивались бы чужим отказом, ничего не сказав о своём.
    failures += _run(guards, "охранники молчат на исправном дереве", not guards(FULL), verbose)
    for title, key, tree in GUARDS:
        tree = {k: v for k, v in tree.items() if v is not None}
        hit = [m for m in guards(tree) if key in m]
        failures += _run(guards, f"охранник: {title}", bool(hit), verbose)
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _fixture_repo(root)
        got = set(scan(root))
        want = {"engine/a.cpp", "docs/examples/c.cpp", "engine/new.cpp"}
        failures += _run(scan, "обход: корни, расширения, индекс и ненаписанное", got == want, verbose)
        if got != want:
            print(f"         лишнее: {sorted(got - want)}; недостача: {sorted(want - got)}")
    total = len(CASES) + len(QUIET) + len(GUARDS) + 2
    if verbose or failures:
        print(f"hash-seam selftest: {'FAIL' if failures else 'PASS'} — "
              f"{total} кейсов, провалов: {failures}")
    return 1 if failures else 0
