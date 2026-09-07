"""Правила шва файлового ввода-вывода: что считать обходом `platform::*` и чем это подавляется.

Граница та же, что у пары `check_hash_seam.py` / `hash_seam_rules.py`: там — обход дерева и
охранники, здесь — суждение о ТЕКСТЕ файла. Обе половины проверяются одним набором
(`check_fs_seam_selftest.py`), разбор литералов и комментариев берётся у `cpp_text.py`.

Конвенция названа в `.context/conventions.md`: файловый ввод-вывод идёт через
`platform::open_file`/`read_text`/`read_bytes`/`remove_file`, а не через узкие CRT-функции. Причина
не стилистическая: `std::fopen` на Windows принимает ANSI, поэтому путь через профиль
`C:\\Users\\Пётр\\` он не открывает ВОВСЕ — а выглядит это как «файла нет». Ровно тем же обманом
ломаются `std::rename`, `std::remove` и конструктор `std::ifstream` от `std::string`.

Для argv и переменных окружения такие греп-гейты заведены давно (`tree_invariants.sh argv|env`),
для файловой половины — не было ни одного, и обходили её девять раз (находка 6 аудита #21).

Имя `remove` — единственное в списке, у которого есть ЗАКОННАЯ форма: `std::remove(first, last,
value)` из `<algorithm>` не имеет к файлам никакого отношения, и в дереве такая форма живёт
(`tools/assetc/bakers_table.cpp`). Различаются они ЧИСЛОМ АРГУМЕНТОВ верхнего уровня, а не именем,
поэтому здесь есть разбор скобок: гейт на подстроке отбивал бы законный алгоритм и требовал бы
отписки за корректный код. `std::remove_if` — другое имя и под правило не подпадает вовсе.

Квалификатор перед именем правило НЕ сужает до `std::`, и это намеренно: `fs::remove(p)`,
`boost::filesystem::remove(p)` и `ns::fopen(p)` ломаются тем же ANSI-барьером, а чужой одноаргументный
`remove` из своего пространства имён — случай на отписку, а не причина ослепить гейт. Метод
(`v.remove(x)`, `p->remove(x)`) правилом не считается: там есть объект, а не путь.
"""
import re

from cpp_text import line_of, marker_lines, split_code_comments

# Файлы шва — и только они. Пути написаны ЯВНО, а не выведены из имени: «файл, в имени которого
# есть fs» — правило, под которое подпадает первая же копия, названная `foo_fs.cpp`.
SEAM = ("engine/platform/platform_fs.hpp",
        "engine/platform/platform_fs_posix.cpp",
        "engine/platform/platform_fs_win32.cpp")

# Узкие CRT-функции, принимающие ПУТЬ, и потоки, чей конструктор принимает его же. Список закрыт и
# перечисляет ровно то, что ANSI-барьер ломает молча: wide-формы (`_wfopen`, `DeleteFileW`) законны
# и живут в шве, а обратная форма — «всё, кроме разрешённого» — потребовала бы перечислить
# стандартную библиотеку целиком.
CALLS = {"fopen": "std::fopen", "freopen": "std::freopen", "tmpnam": "std::tmpnam",
         "rename": "std::rename", "remove": "std::remove",
         # Безопасные формы Microsoft узки ровно так же: суффикс `_s` чинит переполнение буфера, а
         # не кодировку пути. Написать их придёт в голову первому же, кто увидит на MSVC
         # `warning C4996: 'fopen': This function or variable may be unsafe` — дерево объясняет эту
         # диагностику в четырёх местах (`engine/light/bake.cpp`, `engine/material/bake.cpp`), то
         # есть обход уже подсказан компилятором.
         "fopen_s": "fopen_s", "freopen_s": "freopen_s", "tmpnam_s": "tmpnam_s"}
# ANSI-формы Win32 и узкие обёртки CRT. Шапка `platform_fs.hpp` называет их историческим дефектом,
# и живут они там же, где шов: в `engine/platform/*_win32.cpp` (сегодня их одиннадцать, освобождён
# из них один). Список закрыт и перечисляет именно `*A`: wide-формы (`CreateFileW`, `DeleteFileW`)
# законны и составляют вторую половину шва, а безсуффиксное `CreateFile` — макрос, который под `UNICODE`
# разворачивается в правильную половину.
WIN = ("CreateFileA", "CreateDirectoryA", "RemoveDirectoryA", "DeleteFileA", "CopyFileA",
       "MoveFileA", "MoveFileExA", "FindFirstFileA", "GetFileAttributesA", "GetModuleFileNameA",
       "GetCurrentDirectoryA", "SetCurrentDirectoryA", "GetFullPathNameA", "GetTempPathA",
       "_mkdir", "_rmdir", "_unlink", "_access", "_chdir")
# Число аргументов различает файловую форму от алгоритма только у `remove`; прочим именам
# файловость даёт само имя.
ARITY_ONE = ("remove",)
TYPES = {"ifstream": "std::ifstream", "ofstream": "std::ofstream", "fstream": "std::fstream",
         "filesystem": "std::filesystem"}


def _names(names):
    """Альтернативы длинными вперёд: `fopen` иначе съедает начало `fopen_s`."""
    return "|".join(sorted(names, key=len, reverse=True))


# Имя вызова: не метод (`v.remove(x)`), не часть длинного идентификатора (`remove_if`, `_wremove`).
CALL = re.compile(r"(?<![\w.>])(?:\w+\s*::\s*)*(" + _names(CALLS) + r")\s*\(")
WIN_CALL = re.compile(r"(?<![\w.>])(" + _names(WIN) + r")\s*\(")
TYPE = re.compile(r"(?<![\w])(?:std\s*::\s*)?(" + _names(TYPES) + r")(?![\w])")
ALLOW = re.compile(r"fs-seam:\s*allow\b(.*)")

FIX = ("Возьми шов (`platform::open_file`/`read_text`/`read_bytes`/`remove_file`/`replace_file`) "
       "или объясни маркером `// fs-seam: allow <причина в три слова>`.")


def top_level_args(code, open_paren):
    """Число аргументов верхнего уровня вызова, чья открывающая скобка стоит в open_paren.

    Скобка не закрылась до конца файла — считаем один аргумент: обрубок разбора не повод объявлять
    находку, у которой нет формы.
    """
    depth, commas, i, n = 0, 0, open_paren, len(code)
    while i < n:
        c = code[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                return commas + 1 if code[open_paren + 1:i].strip() else 0
        elif c == "," and depth == 1:
            commas += 1
        i += 1
    return 1


def calls(code):
    """Обходы шва вызовом: [(номер строки, объяснение)]."""
    found = []
    for m in CALL.finditer(code):
        name = m.group(1)
        args = top_level_args(code, m.end() - 1)
        if name in ARITY_ONE and args != 1:
            continue
        found.append((line_of(code, m.start()),
                      "`%s` мимо платформенного шва" % CALLS[name]))
    for m in WIN_CALL.finditer(code):
        found.append((line_of(code, m.start()),
                      "`%s` — узкая форма Windows мимо платформенного шва" % m.group(1)))
    return found


def _preproc(code, pos):
    """Стоит ли совпадение на строке препроцессора: `#include <fstream>` типом не пользуется."""
    start = code.rfind("\n", 0, pos) + 1
    return code[start:pos].lstrip().startswith("#")


def types(code):
    """Обходы шва типом (потоки и `std::filesystem`): [(номер строки, объяснение)]."""
    return [(line_of(code, m.start()),
             "`%s` мимо платформенного шва: путь от `std::string` на Windows идёт через "
             "ANSI-кодовую страницу" % TYPES[m.group(1)])
            for m in TYPE.finditer(code) if not _preproc(code, m.start())]


def file_hits(text):
    """Обе половины правила разом: [(номер строки, объяснение)] плюс множество накрытых маркером."""
    code, com = split_code_comments(text)
    return sorted(calls(code) + types(code)), marker_lines(com, ALLOW)


def seam_forms(text):
    """(вызовов шва, алгоритмических форм `remove`) — позитивный контроль на настоящем дереве."""
    code, _ = split_code_comments(text)
    algo = sum(1 for m in CALL.finditer(code)
               if m.group(1) in ARITY_ONE and top_level_args(code, m.end() - 1) != 1)
    return len(calls(code)) + len(types(code)), algo
