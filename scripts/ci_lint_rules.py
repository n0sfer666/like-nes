"""Правила линтера workflow: каждое снято с уже случившейся красной сборки, а не придумано.

Общий принцип: находка описывает КЛАСС отказа, который на локальной машине не воспроизводится
вовсе, а в CI стоит круга по 20+ минут на трёх ОС.
"""
import re

from ci_lint_patterns import (ASSIGN, COUNTER, DIALECTS, MSVC_FLAG, PROBE, SEARCH, SEGMENT,
                              SYSTEM_PATHS, TOKEN, TOOLS)


class Finding:
    def __init__(self, path, line, rule, message):
        self.path, self.line, self.rule, self.message = path, line, rule, message

    def __str__(self):
        return f"{self.path}:{self.line}: [{self.rule}] {self.message}"


def _code(text):
    """Комментарий — `#` в начале слова; `grep '#ifdef'` и `sed 's/#.*//'` кодом остаются."""
    return re.sub(r"(^|\s)#.*$", "", text)


def _dirname(path):
    return path.rstrip("/").rsplit("/", 1)[0]


def _value(text):
    """Из строки атрибута берётся ЗНАЧЕНИЕ: ключ `timeout-minutes:` — это не вызов `timeout`."""
    key, sep, value = text.partition(":")
    return value if sep and re.match(r"^[\s-]*[\w.-]+$", key) else text


def _lines(step):
    """Все строки шага с ОС, на которых они выполняются. `env:` и `with:` наравне с телом `run:`:
    команда прячется в них ровно так же, а правило, смотрящее только в `run:`, этого не видит."""
    return [(n, _value(t), step.os_set) for n, t in step.attrs_lines] + list(step.script)


def rule_unparsed(step):
    """Шаг без `uses:` и без тела — это промах разбора, а не чистый шаг. Без этого правила
    парсер деградирует молча: не разобрал — значит, находок нет — значит, всё хорошо."""
    if not step.script and not re.search(r"^\s+-?\s*uses:", step.attrs, re.M):
        yield Finding(step.path, step.line, "unparsed",
                      f"«{step.name}» не разобран: ни `uses:`, ни тела `run:` — правила по нему "
                      f"не отработали, а не подтвердили чистоту")


def rule_portability(step):
    for lineno, text, os_set in _lines(step):
        code = _code(text)
        for tool, present in TOOLS.items():
            if re.search(rf"(?<![\w/-]){re.escape(tool)}\b", code) \
                    and not step.suppressed(tool, lineno) and os_set - present:
                yield Finding(step.path, lineno, "portability",
                              f"`{tool}` отсутствует на {', '.join(sorted(os_set - present))}, "
                              f"а шаг «{step.name}» там выполняется")
        for token, (pattern, label, present) in DIALECTS.items():
            if re.search(pattern, code) and not step.suppressed(token, lineno) and os_set - present:
                yield Finding(step.path, lineno, "portability",
                              f"`{label}` — GNU-диалект, ломается на "
                              f"{', '.join(sorted(os_set - present))}")


def rule_gate_downgrade(step):
    for scope, text in (("шаге", step.attrs), ("job", step.job_attrs)):
        value = re.search(r"^\s*continue-on-error:\s*(\S+)", text, re.M)
        if value and value.group(1) != "false" and "best-effort" not in step.name \
                and not step.suppressed("gate-downgrade"):
            yield Finding(step.path, step.line, "gate-downgrade",
                          f"«{step.name}»: continue-on-error на {scope} понижает обязательный "
                          f"гейт до warning; шаг должен быть назван best-effort или стать "
                          f"блокирующим")


def rule_env_assumption(step):
    # Проба засчитывается только своему сегменту команды: `ls build && VK_ICD=/usr/…/lvp.json`
    # проверяет build, а не ICD, и раньше снимала правило со всей строки.
    probed = set()
    for _, text, _ in _lines(step):
        for segment in SEGMENT.split(_code(text)):
            if PROBE.search(segment):
                probed |= {_dirname(m.group(0)) for m in SYSTEM_PATHS.finditer(segment)}
    for lineno, text, _ in _lines(step):
        if step.suppressed("env-assumption", lineno):
            continue
        for match in SYSTEM_PATHS.finditer(_code(text)):
            path = match.group(0)
            if _dirname(path) in probed or path.rstrip("/").count("/") < 3:
                continue
            yield Finding(step.path, lineno, "env-assumption",
                          f"путь `{path}` зашит литералом и нигде не проверяется: пропажа файла "
                          f"в образе раннера читается как ошибка кода")


def rule_vacuous_gate(step):
    counted, suspect = set(), None
    for lineno, text, _ in step.script:
        code = _code(text)
        assign = ASSIGN.search(code)
        if not (assign and SEARCH.search(code)):
            continue
        if COUNTER.search(code):
            counted.add(assign.group(1))
        elif suspect is None:
            suspect = (lineno, assign.group(1))
    if suspect is None or step.suppressed("vacuous-gate"):
        return
    lineno, var = suspect
    if not re.search(rf"(\[\[?|test)\s+-[zn]\s+\"?\$\{{?{var}\b", step.body):
        return
    # Порог обязан считать ТОТ ЖЕ поиск. Прежняя проверка «в шаге есть -ge» удовлетворялась любым
    # посторонним сравнением: правило, ловящее ложно-зелёное, само становилось ложно-зелёным.
    if any(re.search(rf"\$\{{?{name}\}}?\"?\s*-(ge|gt|eq)\s+\"?\d", step.body) for name in counted):
        return
    yield Finding(step.path, lineno, "vacuous-gate",
                  f"«{step.name}» падает по непустому результату поиска, но не проверяет, "
                  f"что поиск вообще работает: опечатка в пути даёт вечно зелёный гейт")


def rule_arg_mangling(step):
    """Только `bash`/`sh` на Windows: подмену делает MSYS-рантайм git-bash, а в `pwsh` и `cmd` флаг
    доезжает как написан. Шелл берётся из шага, а при отсутствии — из `defaults.run.shell` job или
    workflow: иначе перевод файла на job-level defaults бесшумно выключил бы правило целиком.

    Осознанный пропуск: шаг `uses:`. Значения его `with:` исполняет чужой композит, объявляющий
    шелл у себя, — предполагать git-bash за него значит ругаться на флаг, который никакой MSYS
    не увидит."""
    if "Windows" not in step.os_set or re.search(r"^\s*-?\s*uses:", step.attrs, re.M):
        return
    declared = re.search(r"^\s+shell:\s*['\"]?(\S+)", step.attrs, re.M)
    shell = declared.group(1) if declared else step.job_shell
    if not shell or not re.match(r"(bash|sh)\b", shell):
        return
    for lineno, text, os_set in _lines(step):
        if "Windows" not in os_set or step.suppressed("arg-mangling", lineno):
            continue
        code = _code(text)
        for match in MSVC_FLAG.finditer(code):
            # Цитируется токен целиком, а не совпадение: `/permissive-` и `/Fdfoo.pdb` регекс
            # обрывает, и человек пошёл бы грепать по строке, которой в файле нет.
            yield Finding(step.path, lineno, "arg-mangling",
                          f"`{TOKEN.match(code, match.start()).group(0)}` в шаге «{step.name}»: "
                          f"git-bash превратит аргумент со слэша в путь Windows — та же опция "
                          f"пишется через дефис")


RULES = (rule_unparsed, rule_portability, rule_gate_downgrade,
         rule_env_assumption, rule_vacuous_gate, rule_arg_mangling)
