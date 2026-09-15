"""Фикстуры шва файлового ввода-вывода: тексты, на которых правило обязано падать и молчать.

Отделены от механики набора (`check_fs_seam_selftest.py`) по тому же признаку, по какому в дереве
разведены `ci_lint_rules.py` и сам линтер: здесь ДАННЫЕ — сломанные и починенные входы, там
прогон, охранники, обход и якорь. Разрез сделан находкой бюджета длины: таблицы кейсов росли
вместе с закрытым списком имён, и файл упёрся бы в мягкий лимит на первом же расширении.

Ключ поиска — подстрока, которую обязана назвать находка. Сравнивать с полным текстом значило бы
пинить формулировку, и первая же правка сообщения красила бы самопроверку, ничего не проверив.
"""
from fs_seam_rules import SEAM, WIN

# Файлы шва стоят в КАЖДОЙ фикстуре с ЖИВЫМ узким вызовом внутри: без него охранник «правило имён
# сломано» валил бы опорные кейсы по чужой причине.
BASE = {SEAM[0]: "std::FILE* open_file(const std::string& p, const char* m);\n",
        SEAM[1]: 'return std::fopen(p.c_str(), m);\nstd::rename(a.c_str(), b.c_str());\n',
        SEAM[2]: "return _wfsopen(w.c_str(), wm.c_str(), _SH_DENYNO);\n"}

REMOVE = "`std::remove` мимо"
FOPEN = "`std::fopen` мимо"
RENAME = "`std::rename` мимо"
STREAM = "`std::ifstream` мимо"
ALLOW3 = "// fs-seam: allow причина ровно из трёх"
# Ровно ТРИ слова и ровно ДВА: граница `min_words` иначе проверялась бы одним словом против
# четырёх, то есть подтверждала бы только то, что счётчик слов вообще есть.
EXACT3 = "// fs-seam: allow чужой временный файл"
EXACT2 = "// fs-seam: allow два слова"

OPEN_FIX = 'platform::open_file(p, "rb");\n'

# (метка, ключ в находке, сломанный файл, починенный файл)
CASES = [
    ("std::remove на пути", REMOVE,
     "std::remove(path.c_str());\n", "platform::remove_file(path);\n"),
    ("неквалифицированный remove", REMOVE,
     "remove(save.c_str());\n", "platform::remove_file(save);\n"),
    # Запятая ВНУТРИ вложенных скобок аргументом верхнего уровня не является: счёт по подстроке
    # принял бы это за алгоритм и пропустил бы обход шва молча.
    ("вложенный вызов с запятой", REMOVE,
     "std::remove(join(dir, name).c_str());\n", "platform::remove_file(join(dir, name));\n"),
    ("многострочный вызов", REMOVE,
     "std::remove(\n    tmp.c_str());\n", "platform::remove_file(\n    tmp);\n"),
    # Литерал в аргументе — самая естественная форма удаления файла, и до фикса `cpp_text` она
    # проезжала молча: пустой след литерала делал вызов безаргументным, то есть «алгоритмом».
    ("литеральный путь", REMOVE,
     'std::remove("tmp.bin");\n', 'platform::remove_file("tmp.bin");\n'),
    ("raw-литерал в аргументе", REMOVE,
     'std::remove(R"(C:/tmp/x)");\n', 'platform::remove_file(R"(C:/tmp/x)");\n'),
    ("комментарий перед литералом", REMOVE,
     'std::remove(/* путь */ "x");\n', 'platform::remove_file(/* путь */ "x");\n'),
    ("std::fopen", FOPEN, 'std::FILE* f = std::fopen(p, "rb");\n',
     'std::FILE* f = platform::open_file(p, "rb");\n'),
    ("неквалифицированный fopen", FOPEN, 'std::FILE* f = fopen(p, "rb");\n',
     'std::FILE* f = platform::open_file(p, "rb");\n'),
    # Квалификатор не сужен до `std::` намеренно: `fs::remove(p)` ломается тем же барьером.
    ("чужой квалификатор", REMOVE, "mylib::remove(path.c_str());\n",
     "platform::remove_file(path);\n"),
    ("std::rename", RENAME, "std::rename(from.c_str(), to.c_str());\n",
     "platform::replace_file(from, to);\n"),
    # Суффикс `_s` чинит переполнение буфера, а не кодировку пути: MSVC подсказывает эту форму
    # диагностикой C4996, то есть следующий обход будет написан именно так.
    ("fopen_s", "`fopen_s` мимо", 'fopen_s(&f, p, "rb");\n',
     'std::FILE* f = platform::open_file(p, "rb");\n'),
    ("freopen_s", "`freopen_s` мимо", 'freopen_s(&f, p, "w", stdout);\n', OPEN_FIX),
    ("tmpnam_s", "`tmpnam_s` мимо", "tmpnam_s(buf, sizeof(buf));\n", OPEN_FIX),
    ("поток по строковому пути", STREAM, "std::ifstream in(path);\n",
     "std::string text;\nplatform::read_text(path, text);\n"),
    ("поток записи", "`std::ofstream` мимо", "std::ofstream out(path);\n",
     "platform::write_text(path, text);\n"),
    ("двусторонний поток", "`std::fstream` мимо", "std::fstream io(path);\n",
     "platform::write_text(path, text);\n"),
    ("std::filesystem", "`std::filesystem` мимо",
     "std::filesystem::remove_all(dir);\n", "platform::remove_tree(dir);\n"),
    ("маркер-отписка не подавляет", REMOVE,
     "std::remove(p.c_str()); // fs-seam: allow надо\n",
     "std::remove(p.c_str()); %s\n" % ALLOW3),
    ("маркер ровно в два слова не подавляет", REMOVE,
     "std::remove(p.c_str()); %s\n" % EXACT2, "std::remove(p.c_str()); %s\n" % EXACT3),
    ("маркер через строку не накрывает", REMOVE,
     "%s\n\nstd::remove(p.c_str());\n" % ALLOW3, "%s\nstd::remove(p.c_str());\n" % ALLOW3),
    # Литералы гасятся ДО поиска комментариев: `"http://x"` иначе открывал бы комментарий до конца
    # строки и прятал бы код, стоящий за ним.
    ("литерал со слэшами не открывает комментарий", REMOVE,
     'const char* u = "http://x"; std::remove(p.c_str());\n',
     'const char* u = "http://x"; platform::remove_file(p);\n'),
    ("маркер внутри литерала не подавляет", REMOVE,
     'const char* s = "%s";\nstd::remove(p.c_str());\n' % ALLOW3,
     "%s\nstd::remove(p.c_str());\n" % ALLOW3),
    # Освобождены ТРИ названных пути, а не «файл, в имени которого есть fs».
    ("файл с похожим именем не освобождён", FOPEN,
     'std::fopen(p, "rb");\n', 'platform::open_file(p, "rb");\n'),
]

# Порча на КАЖДОЕ имя закрытого списка Windows: имя, которое набор не ломает, выпадает из правила
# молча — ровно тот класс, ради которого сверяются копии списка корней дерева.
CASES += [("узкая форма Windows: %s" % n, "`%s` — узкая форма" % n,
           "%s(p.c_str());\n" % n, OPEN_FIX) for n in WIN]

# Чистые входы: находок быть не должно ни одной. Гейт, который ругается на честное дерево, снимают
# целиком — и вместе с ним уходит всё, что он ловил.
QUIET = [
    ("алгоритм std::remove на итераторах",
     "v.erase(std::remove(v.begin(), v.end(), x), v.end());\n"),
    ("тот же алгоритм в три строки",
     "v.erase(std::remove(v.begin(),\n                    v.end(),\n                    x), v.end());\n"),
    ("std::remove_if — другое имя",
     "d.erase(std::remove_if(d.begin(), d.end(), pred), d.end());\n"),
    ("метод контейнера", "list.remove(x);\n"),
    ("wide-форма CRT в имени", "_wremove(w.c_str());\n"),
    ("wide-форма Win32", "DeleteFileW(w.c_str());\n"),
    ("шов", "platform::remove_file(path);\nplatform::open_file(path, \"rb\");\n"),
    ("узкое имя в комментарии",
     "// Через шов, а не fopen: на MSVC он депрекирован и /W4 /WX валит сборку.\n"),
    ("маркер на той же строке",
     "std::remove(p.c_str()); // fs-seam: allow чужой временный файл теста\n"),
    ("маркер на предыдущей строке",
     "// fs-seam: allow чужой временный файл теста\nstd::remove(p.c_str());\n"),
    ("маркер ровно в три слова подавляет", "std::remove(p.c_str()); %s\n" % EXACT3),
    ("узкое имя в строковом литерале", 'const char* s = "std::fopen";\n'),
    # Включение заголовка типом не пользуется: правило, ругающееся на `#include <fstream>`, требует
    # отписки у файла, который потоков не открывает вовсе.
    ("включение <fstream>", "#include <fstream>\n"),
    ("включение <filesystem> с отступом", "  #  include <filesystem>\n"),
]

BAD_PATH = "engine/other/copy.cpp"
LOOKALIKE = "engine/platform/platform_fs_extra.cpp"

FULL = dict(BASE, **{f"engine/f{i}.cpp": "int x = 1;\n" for i in range(70)})
NO_CALL = {p: "std::FILE* open_file(const std::string& p, const char* m);\n" for p in SEAM}

# (метка, ключ в отказе, дерево)
GUARDS = [
    ("файл шва не найден", "не нашёл файл(ы) шва", dict(FULL, **{SEAM[1]: None})),
    ("обход мимо дерева", "при пороге", BASE),
    ("правило имён сломано", "правило имён сломано", dict(FULL, **NO_CALL)),
]
