"""Разбор GitHub-workflow в плоский список шагов с вычисленным множеством ОС.

Полноценный YAML-парсер здесь не нужен и вреден: PyYAML нет в системном python3 на всех трёх
раннерах, а гейт обязан работать без установки чего бы то ни было. Разбирается ровно та форма,
в которой написаны наши workflow, — отступы фиксированы, и отклонение от них заметно глазом.
"""
import re

OS_ALL = frozenset({"Linux", "Windows", "macOS"})
RUNNER_PREFIX = {"ubuntu": "Linux", "windows": "Windows", "macos": "macOS"}
BLOCK_MARKERS = {"", "|", ">", "|-", ">-", "|+", ">+"}


class Step:
    def __init__(self, path, line, name, os_set, attrs_lines, job_attrs, script):
        self.path, self.line, self.name, self.os_set = path, line, name, os_set
        self.attrs_lines, self.job_attrs = attrs_lines, job_attrs
        self.script = script  # [(номер строки, текст, ОС этой строки)]
        self._by_line = {n: t for n, t in attrs_lines}
        self._by_line.update({n: t for n, t, _ in script})

    @property
    def attrs(self):
        return "\n".join(t for _, t in self.attrs_lines)

    @property
    def body(self):
        return "\n".join(t for _, t, _ in self.script)

    def suppressed(self, token, lineno=None):
        """Подавление требует причину после токена: `# ci-lint: allow shasum — шаг только Linux`.
        Причина — минимум три слова: односимвольная отписка пишется ровно так же дёшево, как
        голое имя правила, и через полгода не читается вовсе."""
        pattern = re.compile(rf"ci-lint: allow {re.escape(token)}\s+(?:\S+\s+){{2,}}\S")
        if lineno is None:
            return bool(pattern.search(f"{self.attrs}\n{self.body}"))
        return any(pattern.search(self._by_line.get(n, "")) for n in (lineno, lineno - 1))


def _os_from_condition(expr, base):
    """Выражение GHA `if:` — `runner.os == 'Linux'`, `matrix.os != 'windows-latest'`."""
    eq = {RUNNER_PREFIX.get(m.lower(), m) for m in re.findall(r"==\s*'([^']+)'", expr)}
    ne = {RUNNER_PREFIX.get(m.lower(), m) for m in re.findall(r"!=\s*'([^']+)'", expr)}
    return ((eq & base) if eq else set(base)) - ne


def _job_os(block):
    runs_on = next((l for l in block if re.match(r"\s*runs-on:", l)), "")
    if "matrix.os" in runs_on:
        line = next((l for l in block if re.search(r"^\s+os:\s*\[", l)), "")
        return {RUNNER_PREFIX[p] for p in RUNNER_PREFIX if p in line} or set(OS_ALL)
    for prefix, name in RUNNER_PREFIX.items():
        if prefix in runs_on:
            return {name}
    return set(OS_ALL)


def _shell_os(line, parent):
    match = re.search(r"RUNNER_OS\"?\s*(!?=)=?\s*[\"']?([A-Za-z]+)", line)
    if not match:
        return None
    name = RUNNER_PREFIX.get(match.group(2).lower(), match.group(2))
    return parent - {name} if match.group(1) == "!=" else parent & {name}


def _script_os(lines, step_os):
    """Ветка `if [ "$RUNNER_OS" = "Linux" ]` сужает ОС для своего тела — иначе линтер ругался бы
    на честно защищённый вызов. Понимается только эта форма; всё прочее наследует ОС шага."""
    out, stack, cur = [], [], set(step_os)
    for lineno, text in lines:
        stripped, line_os = text.strip(), None
        opens = re.match(r"(el)?if\b", stripped)
        closes = re.search(r"(^|;)\s*fi\b", stripped)
        if opens and closes:
            # Однострочник `if …; then …; fi` сужает ОС ровно для себя: кадр на нём не заводится,
            # иначе его `fi` не найдётся и остаток скрипта навсегда остался бы «только Linux».
            narrowed = _shell_os(stripped, cur)
            line_os = cur if narrowed is None else narrowed
        elif opens and stripped.startswith("elif") and stack:
            narrowed = _shell_os(stripped, stack[-1])
            cur = stack[-1] if narrowed is None else narrowed
        elif opens:
            # Кадр заводится на КАЖДЫЙ if, даже не про ОС: иначе вложенный `fi` снял бы чужой
            # кадр, и защищённый вызов после него снова выглядел бы кроссплатформенным.
            stack.append(cur)
            narrowed = _shell_os(stripped, cur)
            cur = cur if narrowed is None else narrowed
        elif stripped == "else" and stack:
            cur = stack[-1] - cur if cur != stack[-1] else stack[-1]
        elif closes and stack:
            cur = stack.pop()
        out.append((lineno, text, frozenset(cur if line_os is None else line_os)))
    return out


def _split_step(body, offset):
    """Тело `run:` отделяется от остальных полей шага: `env:` и `with:` линтеру тоже нужны —
    зашитый путь одинаково опасен и в команде, и в переменной окружения."""
    # `run:` бывает и на строке с дефисом (`- run: cmd`) — форма валидная и частая. Промах маски
    # оставлял такой шаг с ПУСТЫМ script, а правила ходят по script: шаг не проверялся вовсе.
    run_at = next((i for i, l in enumerate(body) if re.match(r"^(?: {8}|      - )run:", l)), None)
    if run_at is None:
        return [(offset + i + 1, l) for i, l in enumerate(body)], []
    inline = body[run_at].split("run:", 1)[1].strip()
    script = [(offset + run_at + 1, inline)] if inline not in BLOCK_MARKERS else []
    end = run_at + 1
    for i, text in enumerate(body[run_at + 1:], start=run_at + 1):
        if text.strip() and not text.startswith(" " * 10):
            break
        script.append((offset + i + 1, text))
        end = i + 1
    rest = list(enumerate(body[:run_at])) + list(enumerate(body[end:], start=end))
    return [(offset + i + 1, l) for i, l in rest], script


def parse(path, text):
    lines = text.splitlines()
    jobs_at = next((i for i, l in enumerate(lines) if l.startswith("jobs:")), len(lines))
    starts = [i for i, l in enumerate(lines)
              if i > jobs_at and re.match(r"^  [A-Za-z0-9_-]+:\s*$", l)] + [len(lines)]
    steps = []
    for job_start, job_end in zip(starts, starts[1:]):
        block = lines[job_start:job_end]
        job_os, job_attrs = _job_os(block), "\n".join(l for l in block if re.match(r"^ {4}[a-z]", l))
        marks = [i for i, l in enumerate(block) if l.startswith("      - ")] + [len(block)]
        for number, (a, b) in enumerate(zip(marks, marks[1:]), start=1):
            body = block[a:b]
            attrs_lines, script = _split_step(body, job_start + a)
            attrs = "\n".join(t for _, t in attrs_lines)
            cond = re.search(r"^\s+if:\s*(.+)$", attrs, re.M)
            os_set = _os_from_condition(cond.group(1), job_os) if cond else job_os
            named = re.search(r"name:\s*\"?([^\"\n]+)", body[0]) or re.search(r"uses:\s*(\S+)", attrs)
            steps.append(Step(path, job_start + a + 1,
                              named.group(1).strip() if named else f"шаг {number}",
                              frozenset(os_set), attrs_lines, job_attrs,
                              _script_os(script, os_set)))
    return steps
