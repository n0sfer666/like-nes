#include "manifest.hpp"
#include <cctype>
#include <sstream>

#include "platform_fs.hpp"
#include "plugin_api.h"

const char* dock_name(DockSlot s) {
    switch (s) {
        case DockSlot::Left: return "left";
        case DockSlot::Right: return "right";
        case DockSlot::Bottom: return "bottom";
        default: return "center";
    }
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
    return s;
}

// Исходов у формы значения ТРИ, а не два: строка из одних цифр, не влезающая в `int`, — это
// число, и звать её «не числом» значит посылать владельца файла искать опечатку там, где её нет.
// Чинятся эти два случая по-разному: опечатка — правкой символа, переполнение — другой версией
// вовсе. Переполнение к тому же единственный вход в `catch` ниже: до `stoi` доезжают только цифры
// (ревью аудита #21, A·2·7).
enum class NumForm { Ok, NotDigits, TooBig };

static NumForm parse_uint(const std::string& s, int& out) {
    if (s.empty()) return NumForm::NotDigits;
    for (char c : s) if (c < '0' || c > '9') return NumForm::NotDigits;
    try { out = std::stoi(s); } catch (...) { return NumForm::TooBig; }
    return NumForm::Ok;
}

// Токен, который ХОТЕЛ быть ключом версии: `API=2`, `api` из `api = 2`, `Api=2`. Отдельная
// сущность, потому что отказ «поля нет» на таком файле врёт — версия в нём написана, просто не в
// той форме, в какой её ищут, и совет дописать `api=` посылает дописывать уже написанное. Смотрим
// только строку `plugin`: `api=` в строке `panel` или отдельной строкой идентичность файла не
// объявляет вовсе, и там «поля нет» — правда (ревью аудита #21, A·2·7).
static bool looks_like_api(const std::string& w) {
    if (w.size() < 3) return false;
    std::string head = w.substr(0, 3);
    for (char& c : head) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return head == "api";
}

static DockSlot parse_dock(const std::string& v) {
    if (v == "left") return DockSlot::Left;
    if (v == "right") return DockSlot::Right;
    if (v == "bottom") return DockSlot::Bottom;
    return DockSlot::Center;
}

Manifest parse_manifest(const std::string& path) {
    Manifest m;
    std::string text;
    if (!platform::read_text(path, text)) { m.error = "cannot open " + path; return m; }
    std::istringstream in(text);

    // Текст поля `api=` держится РЯДОМ с разобранным числом, а не вместо него: у отказа причин
    // несколько — поля нет вовсе, значение не число, число не влезло, число чужое, — и чинятся они
    // по-разному. На одном `api_version == 0` первые три неотличимы: столько же даёт и забытое
    // поле, и `api=v99…`, у которого `parse_uint` отказал, а его возврат до сих пор выбрасывался
    // (аудит #21, A·2·7).
    bool api_seen = false;
    NumForm api_form = NumForm::NotDigits;
    std::string api_text;

    // Идентичность у манифеста одна, поэтому вторая строка `plugin` (и повторный ключ `api=` в
    // одной строке) — отказ, а не «побеждает последний». Молчаливое правило приоритета обходило бы
    // вердикт о версии одной лишней строкой: `m.id` доставался последней строке, а `api=` —
    // первой, и файл чужой версии становился бы `ok`. Причина запоминается ПЕРВАЯ найденная и
    // отдаётся в конце вместе с остальными: у отказа один выход (ревью аудита #21, A·2·7).
    bool plugin_seen = false;
    std::string dup_error;
    std::string near_miss;

    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::istringstream ls(t);
        std::string tok;
        ls >> tok;
        if (tok == "plugin") {
            if (plugin_seen && dup_error.empty()) dup_error = "manifest names a second plugin line";
            plugin_seen = true;
            // Ключи `key=value` снимаются со строки НЕЗАВИСИМО от позиции, а не после двух
            // позиционных полей: у `plugin id api=1 api=2` поля версии нет, `>> m.version`
            // съедал первый ключ, до счёта повторов доезжал только второй — и файл, объявивший
            // чужую версию, принимался как своей, ровно тем обходом, ради запрета которого
            // правило о повторе и заведено (ревью аудита #21, A·2·7).
            std::string word;
            int positional = 0;
            while (ls >> word) {
                if (word.rfind("api=", 0) == 0) {
                    if (api_seen && dup_error.empty()) dup_error = "manifest repeats api=";
                    api_seen = true;
                    api_text = word.substr(4);
                    api_form = parse_uint(api_text, m.api_version);
                    continue;
                }
                if (near_miss.empty() && looks_like_api(word)) near_miss = word;
                if (word.find('=') != std::string::npos) continue; // чужой ключ, не поле
                if (positional == 0) m.id = word;
                else if (positional == 1) m.version = word;
                ++positional;
            }
        } else if (tok == "panel") {
            PanelDecl p;
            ls >> p.id;
            std::string rest;
            std::getline(ls, rest);
            rest = trim(rest);
            std::string dock;
            if (!rest.empty() && rest.front() == '"') {
                size_t close = rest.find('"', 1);
                if (close != std::string::npos) {
                    p.title = rest.substr(1, close - 1);
                    std::string tail = trim(rest.substr(close + 1));
                    size_t dpos = tail.rfind("dock=");
                    if (dpos != std::string::npos) dock = trim(tail.substr(dpos + 5));
                } else {
                    p.title = unquote(rest);
                }
            } else {
                size_t dpos = rest.rfind("dock=");
                if (dpos != std::string::npos) {
                    dock = trim(rest.substr(dpos + 5));
                    rest = trim(rest.substr(0, dpos));
                }
                p.title = rest;
            }
            p.dock = parse_dock(dock);
            m.panels.push_back(p);
        } else if ((tok == "text" || tok == "button" || tok == "checkbox" || tok == "slider")
                   && !m.panels.empty()) {
            WidgetDecl w;
            if (tok == "text") w.kind = WidgetKind::Text;
            else if (tok == "button") w.kind = WidgetKind::Button;
            else if (tok == "checkbox") w.kind = WidgetKind::Checkbox;
            else w.kind = WidgetKind::SliderInt;
            std::string rest;
            std::getline(ls, rest);
            w.label = unquote(trim(rest));
            m.panels.back().widgets.push_back(w);
        }
    }
    m.ok = !m.id.empty() && !m.panels.empty();
    if (!m.ok) {
        if (m.error.empty()) m.error = "no plugin id or no panels in " + path;
        return m;
    }
    // Версия ABI сверяется ЗДЕСЬ — там же, где поле разбирается, и это единственное место, где
    // «до dlopen» держится по построению, а не по дисциплине вызывающего. Настоящий гейт версии
    // живёт в `host.cpp` (`plugin_abi_version() != PLUGIN_API_VERSION`) и работает верно, но
    // спрашивает уже ПОСЛЕ открытия модуля — то есть после того, как чужой код отработал свои
    // статические инициализаторы. А поле манифеста до сих пор разбиралось и выбрасывалось:
    // читатель видел объявленную версию и принимал её за работающую проверку (аудит #21, A·2·7).
    //
    // Отказ отдаётся тем же `ok`/`error`, что и структурный: у манифеста один вход, и второй способ
    // сказать «не берите этот файл» вызывающие обходили бы по одному. Сегодня потребитель один
    // (`ui_shell_main`), и он уже печатает `error`; путь «манифест → dlopen» получит отказ даром.
    //
    // Каждый отказ называет ФАЙЛ, о котором говорит, как и два структурных выше: поле `error` одно,
    // а потребитель у него общий, и правило именования субъекта в нём не может быть двойным —
    // иначе тот, кто логирует только `error` (а это и есть сегодняшний гейт), теряет предмет.
    m.ok = false;
    if (!dup_error.empty())
        m.error = dup_error + " in " + path;
    else if (!api_seen) {
        m.error = "manifest declares no api= in " + path;
        if (!near_miss.empty()) m.error += ", though its plugin line says '" + near_miss + "'";
    }
    else if (api_form == NumForm::NotDigits)
        // Значение берётся в кавычки: пустое `api=` иначе давало бы сообщение, обрывающееся на
        // двоеточии, — то есть диагностику, по которой не видно, что именно написано в файле.
        m.error = "manifest api= is not a number: '" + api_text + "' in " + path;
    else if (api_form == NumForm::TooBig)
        m.error = "manifest api= does not fit a version number: '" + api_text + "' in " + path;
    else if (m.api_version != PLUGIN_API_VERSION)
        m.error = "manifest api=" + std::to_string(m.api_version) + " but host wants " +
                  std::to_string(PLUGIN_API_VERSION) + ", in " + path;
    else
        m.ok = true;
    return m;
}
