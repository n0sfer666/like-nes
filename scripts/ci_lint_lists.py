"""Сверка руками написанных списков в workflow с их источником правды.

Класс снят с красной сборки 2026-08-23. Санитайзерный шаг физики компилирует модуль НЕ через
CMake, а своим списком единиц трансляции в переменной `LIB=`; новый `query_index.cpp` попал в
цель CMake и не попал в этот список. Сборочный гейт такого не видит по устройству — он идёт
через CMake, где файл зарегистрирован, — поэтому находка стоила круга CI на трёх ОС и вылезла
единственным способом: `undefined reference` на линковке единственного раннера, где шаг вообще
запускается.

Сверка ОПТ-ИН и точечная: список объявляет свой эталон комментарием прямо над собой, потому что
общего правила «список обязан совпадать с каталогом» не существует. Соседний `CHAR=` в том же
шаге перечисляет четыре файла из восьми осознанно — тесту нужен минимальный набор для линковки,
а не зеркало цели, и равенство отбивало бы его ложно.

Две формы эталона, потому что расходятся они по-разному:
  `# ci-lint: mirrors-target <CMakeLists.txt> <цель>` — список ОБЯЗАН содержать все исходники
  цели (сверх них он вправе нести чужие файлы: тот же `LIB=` тащит два файла из framework/core),
  и не вправе нести файл из каталога цели, которого в цели нет, — это переименованный или
  удалённый исходник.
  `# ci-lint: mirrors-var <файл.sh> <ПЕРЕМЕННАЯ>` — РАВЕНСТВО множеств. Так связаны список целей
  Debug-шага и `STATE_TARGETS` в `check_goldens.sh`: они обязаны совпадать в обе стороны, и до
  сих пор это держалось абзацем в CLAUDE.md, то есть дисциплиной.
  `# ci-lint: mirrors-group <имя>` — равенство списков МЕЖДУ СОБОЙ, без внешнего эталона. Списку
  швов его взять неоткуда: он перечисляет posix-ветку платформы, то есть подмножество цели, и
  `mirrors-target` отбивал бы его ложно. Копий у него три (`PLAT_FS` в двух шагах, `TILES_FS` в
  третьем), а сверял их до 2026-08-24 никто: красная сборка того же дня — `undefined reference
  to platform::root_len` — это ровно расхождение одной копии с двумя другими.

Группа из ОДНОГО списка — находка, а не молчание: вторая копия переименована или удалена, и
равенство, которому не с чем сравнивать, зелено вакуумно.
"""
import re

from ci_lint_rules import Finding

MARKER = re.compile(r"#\s*ci-lint:\s*mirrors-(target|var|group)\s+(\S+)(?:\s+(\S+))?")
ASSIGN = re.compile(r"^\s*([A-Za-z_][\w-]*)\s*[:=]\s*\"([^\"]*)\"")
SOURCE = re.compile(r".+\.(?:c|cc|cpp|mm)$")
CMAKE_KEYWORDS = frozenset({"STATIC", "SHARED", "MODULE", "OBJECT", "INTERFACE", "ALIAS",
                            "IMPORTED", "EXCLUDE_FROM_ALL", "WIN32", "MACOSX_BUNDLE"})
RULE = "list-drift"


def markers(text):
    """Пары «маркер → список под ним». Список ищется в ближайших строках ПОД маркером, а не в
    той же: `LIB=` в шелле и `LIKE_NES_BUILD_TARGETS:` в `env:` пишутся по-разному, но маркер
    над ними стоит одинаково."""
    lines = text.splitlines()
    found = []
    for i, line in enumerate(lines):
        m = MARKER.search(line)
        if not m:
            continue
        for j in range(i + 1, min(i + 4, len(lines))):
            a = ASSIGN.match(lines[j])
            if a:
                found.append((i + 1, m.group(1), m.group(2), m.group(3), a.group(1),
                              a.group(2).split()))
                break
        else:
            found.append((i + 1, m.group(1), m.group(2), m.group(3), None, None))
    return found


def cmake_target_sources(text, target):
    """Исходники цели `add_library`/`add_executable`. Скобки считаются, потому что список
    переносится на несколько строк, а закрывающая живёт вплотную к последнему файлу."""
    m = re.search(rf"add_(?:library|executable)\s*\(\s*{re.escape(target)}\b", text)
    if not m:
        return None
    depth, out, i = 1, [], m.end()
    while i < len(text) and depth:
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if not depth:
                break
        i += 1
    for token in text[m.end():i].split():
        if token not in CMAKE_KEYWORDS and SOURCE.match(token):
            out.append(token)
    return out


def shell_var_tokens(text, name):
    m = re.search(rf"^\s*{re.escape(name)}=\"([^\"]*)\"", text, re.M)
    return None if m is None else m.group(1).split()


def drift(kind, listed, reference, namespace):
    """Ядро суждения, отделённое от чтения файлов: фикстуры самопроверки гоняют его без
    единого обращения к диску."""
    listed_set, reference_set = set(listed), set(reference)
    out = [f"{p} — есть в эталоне, нет в списке" for p in sorted(reference_set - listed_set)]
    extra = sorted(t for t in listed_set - reference_set
                   if kind == "var" or (namespace and t.startswith(namespace)))
    out += [f"{p} — есть в списке, нет в эталоне" for p in extra]
    return out


def analyze(path, text, read):
    """`read` — путь эталона → его текст или None. Файл-эталон, которого нет, это находка, а не
    молчание: переехавший путь гасит сверку ровно так же, как её отсутствие."""
    findings = []
    for line, kind, ref_path, ref_name, list_name, listed in markers(text):
        if kind == "group":
            continue
        if list_name is None:
            findings.append(Finding(path, line, RULE,
                                    "маркер сверки не нашёл под собой списка в кавычках"))
            continue
        source = read(ref_path)
        if source is None:
            findings.append(Finding(path, line, RULE, f"эталон {ref_path} не прочитан"))
            continue
        namespace = ""
        if kind == "target":
            namespace = ref_path.rstrip("/").rsplit("/", 1)[0] + "/"
            reference = cmake_target_sources(source, ref_name)
            reference = None if reference is None else [namespace + s for s in reference]
        else:
            reference = shell_var_tokens(source, ref_name)
        if not reference:
            findings.append(Finding(path, line, RULE,
                                    f"в {ref_path} не найдено ни одного элемента {ref_name}"))
            continue
        for message in drift(kind, listed, reference, namespace):
            findings.append(Finding(path, line, RULE, f"{list_name} ↔ {ref_name}: {message}"))
    return findings


def group_drift(entries):
    """Суждение о группе, как и `drift`, отделено от чтения дерева: на вход приходят уже добытые
    списки. Виноватого оно не называет — при двух копиях расхождение симметрично, и обе стороны
    равно вправе быть правой; поэтому находка печатается на КАЖДОМ списке, где элемента нет."""
    if len(entries) < 2:
        path, line, list_name, _ = entries[0]
        return [Finding(path, line, RULE,
                        f"{list_name}: в группе единственный список, сверять не с чем")]
    union = set().union(*(set(tokens) for _, _, _, tokens in entries))
    return [Finding(path, line, RULE,
                    f"{list_name}: {token} — есть в остальных списках группы, нет здесь")
            for path, line, list_name, tokens in entries
            for token in sorted(union - set(tokens))]


def check(files, read):
    """Позитивный контроль на саму сверку: ноль маркеров в дереве — это переехавший формат
    комментария, а не «сверять нечего», и читается такое молчание как чистый прогон. Чтение
    приходит снаружи параметром, поэтому контроль проверяется фикстурой без единого файла."""
    findings, seen, groups = [], 0, {}
    for rel in files:
        text = read(rel)
        for line, kind, name, _ref, list_name, listed in markers(text):
            seen += 1
            if kind != "group":
                continue
            if list_name is None:
                findings.append(Finding(rel, line, RULE,
                                        "маркер сверки не нашёл под собой списка в кавычках"))
                continue
            groups.setdefault(name, []).append((rel, line, list_name, listed))
        findings += analyze(rel, text, read)
    for name in sorted(groups):
        findings += group_drift(groups[name])
    if not seen:
        findings.append(Finding(".github/workflows", 0, RULE,
                                "ни одного маркера сверки: гейт ничего не проверил"))
    return findings
