// Цель гейта 9: манифест плагина. Стоит ОТДЕЛЬНО от прочих, потому что единственная читает не
// буфер, а ФАЙЛ: `parse_manifest` принимает путь и сам зовёт `platform::read_text`. Это ровно тот
// вход, что приезжает от пользователя вместе с чужим плагином, поэтому цель важна — но мутант
// приходится класть на диск, и дисковую возню держим в своей единице трансляции.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "fuzz_io_span.hpp"
#include "fuzz_target.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "plugin/manifest.hpp"

namespace fuzz {
namespace {

// Семя — ЛИТЕРАЛ, а не копия файла с диска: у манифеста нет пекаря, и выбирать приходится между
// копией текста и зависимостью гейта от раскладки дерева на раннере. Копия молчать не умеет:
// прогон скармливает цели её собственное семя ПЕРВЫМ и печатает «its own valid seed was refused»,
// если формат уехал, — то есть протухание видно сразу и с именем цели.
const char* const MANIFEST_SRC =
    "plugin console 1.0 api=2\n"
    "\n"
    "panel console \"Console\" dock=bottom\n"
    "  text \"[host] plugin system registered\"\n"
    "  button \"Clear\"\n"
    "  checkbox \"Wrap\"\n"
    "  slider \"Depth\" 0 64\n"
    "\n"
    "panel viewport \"Viewport\" dock=center\n"
    "  text \"sim golden-hash\"\n";

std::vector<uint8_t> seed_manifest() {
    const std::string s(MANIFEST_SRC);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// Путь рядом с рабочим каталогом прогона, а не в системном temp: раннеры Windows отдают под temp
// путь с пробелами и кириллицей, и отказ читался бы как дефект разбора.
// Имя с идентификатором процесса, а не постоянное: два прогона в одном каталоге (этап preflight и
// ручной повтор рядом) затирали бы файл друг друга и давали ложный FAIL. Падение или ^C файл всё
// равно оставляет, поэтому шаблон занесён в `.gitignore`: чистоту `git status` утверждает гейт 10
// спеки #20, и untracked-мусор в корне ронял бы ЕГО, а не этот гейт.
const std::string& temp_path() {
    static const std::string path =
        "fuzz_manifest_" + std::to_string(platform::process_id()) + ".tmp";
    return path;
}

// Отказ ОКРУЖЕНИЯ, а не формата. Каталог только на чтение, полный диск или квота дали бы отказ на
// каждом случае, цель не приняла бы ни одного мутанта, и вердиктом стала бы строка про вакуум —
// гейт обвинил бы разбор манифеста в том, чего тот не делал, и дефект искали бы в парсере. Поэтому
// причина называется своим текстом, и прогон кончается здесь же: следующий случай упрётся в то же.
// `[[noreturn]]` — не украшение: без него поток управления за `if (f == nullptr) die_io(...)`
// синтаксически продолжается в `fwrite` с нулевым `FILE*`, и следующая правка (или статический
// анализатор MSVC) прочтёт это как разыменование нуля.
[[noreturn]] void die_io(const char* what) {
    std::fprintf(stderr, "[fuzz] FAIL plugin-manifest: %s %s, no case ever ran\n", what,
                 temp_path().c_str());
    std::fflush(stderr);
    // Вердикты уже прошедших целей лежат в stdout, и `_Exit` их не сбросит. Снятый в `main` буфер
    // тут не оправдание: возврат `setvbuf` не проверяется, поэтому сброс здесь ЯВНЫЙ.
    std::fflush(stdout);
    std::_Exit(1);
}

bool read_manifest(const uint8_t* data, size_t size) {
    {
        // Запись — ДИСК, а не разбор, и под порогом случая ей делать нечего (см.
        // `fuzz_io_span.hpp`). Область закрывается ПОСЛЕ `fclose`, потому что закрытие тоже диск,
        // и на Windows оно самое дорогое: антивирус сканирует файл ровно на закрытии.
        fuzz::IoSpan io;
        std::FILE* f = platform::open_file(temp_path(), "wb");
        if (f == nullptr) die_io("cannot open");
        // Возвраты сверяются ОБА: короткая запись (полный диск, квота) и ошибка на сбросе буфера
        // в `fclose` отдали бы парсеру усечённый файл, тот вернул бы `!m.ok`, и причина была бы
        // списана на формат — та же дыра, что и неоткрытый файл, только тише.
        if (size != 0 && std::fwrite(data, 1, size, f) != size) {
            std::fclose(f);
            die_io("short write to");
        }
        if (std::fclose(f) != 0) die_io("cannot close");
    }

    const Manifest m = parse_manifest(temp_path());
    {
        fuzz::IoSpan cleanup;
        platform::remove_file(temp_path());
    }
    if (!m.ok) return false;

    consume_all(m.id.size(), m.version.size(), m.api_version);
    for (const PanelDecl& p : m.panels) {
        consume_all(p.id.size(), p.title.size());
        consume(std::strlen(dock_name(p.dock)));
        for (const WidgetDecl& w : p.widgets) {
            consume_all(w.kind, w.label.size());
            consume_all(w.min, w.max);
        }
    }
    return true;
}

const Target TARGETS[] = {
    {"plugin-manifest", seed_manifest, read_manifest},
};

} // namespace

const Target* plugin_targets(std::size_t* count) {
    *count = sizeof(TARGETS) / sizeof(TARGETS[0]);
    return TARGETS;
}

} // namespace fuzz
