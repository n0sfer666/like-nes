"""Фикстуры запрета C-style кастов: правило обязано сработать на поломке и промолчать на починке.

Контракт тот же, что у `check_include_seam_selftest.py`: правило проверяется словарём «путь →
текст», охранники — своим, обход — НАСТОЯЩИМ репозиторием, потому что берёт файлы у git. Ключ
поиска — подстрока, которую обязана назвать находка. Тихие входы собраны из форм, которые текстом
похожи на каст и кастом не являются: на каждой из них эвристика ошибалась бы молча.
"""
import subprocess
import tempfile
from pathlib import Path

# (метка, ключ в находке, сломанный текст, починенный текст)
CASES = [
    ("встроенный тип", "(int)", "int f(double v) { return (int)v; }\n",
     "int f(double v) { return static_cast<int>(v); }\n"),
    ("тип перед скобкой выражения", "(uint8_t)", "p[0] = (uint8_t)(base * 235);\n",
     "p[0] = static_cast<uint8_t>(base * 235);\n"),
    ("многословный встроенный тип", "(unsigned long long)",
     'printf("%llu", (unsigned long long)h);\n', 'printf("%llu", static_cast<unsigned long long>(h));\n'),
    ("указатель", "(App*)", "App& a = *(App*)app->userData;\n",
     "App& a = *static_cast<App*>(app->userData);\n"),
    ("указатель на const", "(const char*)", "use((const char*)buf);\n",
     "use(reinterpret_cast<const char*>(buf));\n"),
    ("квалифицированное имя", "(ns::Id)", "x = (ns::Id)raw;\n", "x = static_cast<ns::Id>(raw);\n"),
    ("шаблонный тип", "(std::vector<int>*)", "v = (std::vector<int>*)p;\n",
     "v = static_cast<std::vector<int>*>(p);\n"),
    ("ветвь тернарного", "(float)", "x = c ? (float)a : b;\n", "x = c ? static_cast<float>(a) : b;\n"),
    ("операнд на следующей строке", "(unsigned long long)", "printf(f, (unsigned long long)\n  n);\n",
     "printf(f, static_cast<unsigned long long>(\n  n));\n"),
    # Внутренний каст цепочки стоит за `)` — ровно там, где правило иначе видит вызов.
    ("внутренний каст цепочки", "(intptr_t)", "k((int)(intptr_t)t);\n",
     "k(static_cast<int>(reinterpret_cast<intptr_t>(t)));\n"),
    ("каст внутри .mm", "(CAMetalLayer*)", "CAMetalLayer* l = (CAMetalLayer*)self.view.layer;\n",
     "CAMetalLayer* l = static_cast<CAMetalLayer*>(self.view.layer);\n"),
    ("каст ObjC-сообщения", "(NSString*)", "NSString* s = (NSString*)[d objectForKey:k];\n",
     "NSString* s = static_cast<NSString*>([d objectForKey:k]);\n"),
    ("литерал с точкой", "(float)", "x = (float).5f;\n", "x = static_cast<float>(.5f);\n"),
    ("операнд с префиксным инкрементом", "(int)", "k = (int)++i;\n", "k = static_cast<int>(++i);\n"),
    # `>` перед скобкой — конец шаблона лишь вплотную к ней; с пробелом это сравнение или сдвиг.
    ("правая часть сравнения", "(int)", "if (n > (int)v.size()) f();\n",
     "if (n > static_cast<int>(v.size())) f();\n"),
    ("правая часть сдвига", "(int)", "x = a >> (int)b;\n", "x = a >> static_cast<int>(b);\n"),
    ("аргумент ObjC-сообщения", "(int)", "id n = [NSNumber numberWithInt:(int)v];\n",
     "id n = [NSNumber numberWithInt:static_cast<int>(v)];\n"),
    ("заведомый тип с унарным минусом", "(uint32_t)", "m = (uint32_t)-1;\n",
     "m = static_cast<uint32_t>(-1);\n"),
    ("тип Win32 с побитовым не", "(DWORD)", "m = (DWORD)~0u;\n", "m = static_cast<DWORD>(~0u);\n"),
    ("тело макроса", "(uint32_t)", "#define MASK (uint32_t)1\n", "#define MASK static_cast<uint32_t>(1)\n"),
    ("после co_await", "(int)", "co_await (int)x;\n", "co_await static_cast<int>(x);\n"),
    ("квалифицированный заведомый тип", "(std::size_t)", "m = (std::size_t)-1;\n",
     "m = static_cast<std::size_t>(-1);\n"),
    ("операнд от глобального пространства", "(int)", "k = (int)::abs(v);\n",
     "k = static_cast<int>(::abs(v));\n"),
    # Объявление метода кончается `{`: строка сообщения за ним — уже не параметр.
    ("продолжение ObjC-сообщения", "(int)", "- (void)f:(int)a {\n  [o foo:x\n      with:(int)y];\n}\n",
     "- (void)f:(int)a {\n  [o foo:x\n      with:static_cast<int>(y)];\n}\n"),
    ("ветвь тернарного в .mm", "(int)", "x = c ? a: (int)b;\n", "x = c ? a: static_cast<int>(b);\n"),
]
MM = {"каст внутри .mm", "каст ObjC-сообщения", "аргумент ObjC-сообщения", "продолжение ObjC-сообщения",
      "ветвь тернарного в .mm"}

# Чистые входы: находок быть не должно ни одной.
QUIET = [
    ("отбрасывание значения (void)", "(void)argc;\n(void) w;\n"),
    ("объявление с параметром", "void f(int);\nvoid g(const Foo&) const;\n"),
    ("оператор сравнения", "bool operator==(const Body&) const = default;\n"),
    ("условие и цикл", "if (ok) run();\nwhile (a) b();\nfor (x) y;\nswitch (k) {}\n"),
    ("sizeof и decltype", "n = sizeof(int) * k;\ndecltype(x) y = x;\nalignas(T) char b[4];\n"),
    ("выражение в скобках перед оператором",
     "x = (a) - b;\ny = (a) * b;\nz = (a) && b;\nw = (a)++;\nv = (p)->q;\n"),
    ("выражение в скобках перед точкой с запятой", "return (a);\nthrow (e);\n"),
    ("лямбда", "auto f = [](int x) { return x; };\n"),
    ("тип функции в шаблоне", "std::function<void(int)> cb;\n"),
    ("указатель на функцию", "void (*fn)(int) = nullptr;\n"),
    ("каст в комментарии и строке", '// (int)x\n/* (float)y */\nputs("(int)z");\n'),
    ("размещающий new", "auto* p = new (buf) Body{};\n"),
    ("тип в скобках аргументом макроса", "REGISTER((Body*), (int), name);\n"),
    ("мостовой каст ARC", "id x = (__bridge id)ptr;\n"),
    ("объявление метода Objective-C", "- (BOOL)application:(UIApplication*)app opts:(NSDictionary*)o {\n"),
    ("продолжение объявления Objective-C", "- (void)touchesBegan:(NSSet*)t\n    withEvent:(UIEvent*)e {\n"),
    ("простое имя в конце строки", "ok = (ready)\n  && other;\nx = c ? (a)\n  : b;\n"),
    ("константа в верхнем регистре", "n = (MAX) - 1;\n"),
    ("вызов шаблона", "y = f<T>(x);\nz = get<Enemy>(e) + 1;\n"),
    ("параметры макроса", "#define ID(x) (x)\n#define SQ(a) ((a) * (a))\n"),
]
QUIET_MM = {"объявление метода Objective-C", "мостовой каст ARC", "продолжение объявления Objective-C"}

FULL = {f"engine/core/f{i}.cpp": "x = static_cast<int>(y);\nf(int);\n" * 5 for i in range(70)}

# (метка, ключ в отказе, дерево)
GUARDS = [
    ("обход мимо дерева", "файл(ов) при пороге", {"engine/core/a.cpp": ""}),
    ("регулярка типа сломана", "скобок с типом при пороге", {k: "x = y;\n" for k in FULL}),
]


def _path(title, mm):
    return "platform/ios/view.mm" if title in mm else "engine/core/a.cpp"


def _fixture_repo(root):
    """Настоящий репозиторий: обход берёт файлы у git, а не у os.walk."""
    for rel in ("engine/a.cpp", "engine/b.c", "engine/c.mm", "engine/d.hpp", "engine/e.m",
                "engine/skip.cpp", "engineering/f.cpp", "tools/g.cpp"):
        (root / rel).parent.mkdir(parents=True, exist_ok=True)
        (root / rel).write_text("int x;\n", encoding="utf-8")
    (root / ".gitignore").write_text("engine/skip.cpp\n", encoding="utf-8")
    subprocess.run(["git", "init", "-q"], cwd=root, check=True)
    subprocess.run(["git", "add", "-A"], cwd=root, check=True)
    (root / "engine/new.inl").write_text("int n;\n", encoding="utf-8")


def _run(title, want, verbose):
    if verbose or not want:
        print(f"  [{'PASS' if want else 'FAIL'}] {title}")
    return 0 if want else 1


def selftest(audit, guards, scan, verbose=True):
    failures = 0
    for title, key, bad, ok in CASES:
        path = _path(title, MM)
        fired = [f for f in audit({path: bad}) if key in f]
        silent = audit({path: ok})
        failures += _run(title, bool(fired) and not silent, verbose)
        if not fired:
            print("         правило промолчало на сломанной фикстуре")
        for finding in silent:
            print(f"         правило сработало на починенной фикстуре: {finding}")
    for title, text in QUIET:
        found = audit({_path(title, QUIET_MM): text})
        failures += _run(f"no-false-positive: {title}", not found, verbose)
        for finding in found:
            print(f"         лишняя находка: {finding}")
    # Опорный кейс охранников идёт ПЕРВЫМ: не пройди его исправное дерево — порчи ниже отбивались
    # бы чужим отказом, ничего не сказав о своём.
    failures += _run("охранники молчат на исправном дереве", not guards(FULL), verbose)
    for title, key, tree in GUARDS:
        failures += _run(f"охранник: {title}", any(key in m for m in guards(tree)), verbose)
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _fixture_repo(root)
        got = set(scan(root, ["engine"]))
        want = {"engine/a.cpp", "engine/c.mm", "engine/d.hpp", "engine/new.inl"}
        failures += _run("обход: корни, расширения C++, индекс и ненаписанное", got == want, verbose)
        if got != want:
            print(f"         получено: {sorted(got)}")
    total = len(CASES) + len(QUIET) + len(GUARDS) + 2
    if verbose or failures:
        print(f"c-casts selftest: {'FAIL' if failures else 'PASS'} — {total} кейсов, провалов: {failures}")
    return 1 if failures else 0
