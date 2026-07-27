"""Правила линтера workflow: каждое снято с уже случившейся красной сборки, а не придумано.

Общий принцип всех четырёх: находка описывает КЛАСС отказа, который на локальной машине не
воспроизводится вовсе, а в CI стоит круга по 20+ минут на трёх ОС.
"""
import re

# Инструмент -> ОС, на которых он есть у раннера GitHub. Windows означает git-bash: это НЕ POSIX,
# в нём нет ни perl-скриптов вроде shasum, ни половины GNU-специфичных флагов. Сомнение всегда
# трактуется как «нет»: пропущенный вызов стоит красного прогона, лишняя находка — одной строки.
TOOLS = {
    "shasum": {"Linux", "macOS"},
    "md5sum": {"Linux"},
    "sha256sum": {"Linux"},
    "nproc": {"Linux"},
    "timeout": {"Linux"},
    "ldd": {"Linux"},
    "readelf": {"Linux"},
    "objdump": {"Linux"},
    "ldconfig": {"Linux"},
    "strace": {"Linux"},
    "setsid": {"Linux"},
    "apt-get": {"Linux"},
    "sudo": {"Linux"},
    "dpkg": {"Linux"},
    "xvfb-run": {"Linux"},
    "vulkaninfo": {"Linux"},
    "otool": {"macOS"},
    "install_name_tool": {"macOS"},
    "codesign": {"macOS"},
    "sw_vers": {"macOS"},
    "lipo": {"macOS"},
    "plutil": {"macOS"},
    "hdiutil": {"macOS"},
    "dtruss": {"macOS"},
}
# Не отсутствие команды, а расхождение диалектов: одноимённая утилита ведёт себя иначе.
DIALECTS = {
    "sed-i": (r"sed +-i +(?!\.)", "sed -i без суффикса", {"Linux", "Windows"}),
    "grep-P": (r"grep +-[a-zA-Z]*P\b", "grep -P", {"Linux", "Windows"}),
    "stat-c": (r"stat +-c\b", "stat -c", {"Linux", "Windows"}),
}
SYSTEM_PATHS = re.compile(r"(?<![\w/])(?:/usr/|/etc/|/opt/|/Library/|/Applications/|/System/)\S*")
# Проба существования — только то, что действительно спрашивает файловую систему. Идиома
# `… || { echo; exit 1; }` пробой НЕ считается: это обработчик ошибки, он есть почти в каждом шаге
# и, будучи признан проверкой, выключал бы правило на всём файле.
PROBE = re.compile(r"\bls\b|\bfind\b|\[\[?\s+-[efdrx]\s|test +-[efdrx] ")
SEGMENT = re.compile(r"&&|\|\||;|\|")
SEARCH = re.compile(r"\bgrep\b|\brg\b")
ASSIGN = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=\$\(")
COUNTER = re.compile(r"wc\s+-l|grep\s+-[a-zA-Z]*c\b")


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


RULES = (rule_unparsed, rule_portability, rule_gate_downgrade,
         rule_env_assumption, rule_vacuous_gate)
