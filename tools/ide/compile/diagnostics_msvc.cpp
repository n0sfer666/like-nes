#include "diagnostics_line.hpp"

#include <cstdlib>

namespace ide::build {
namespace {

bool all_digits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    return true;
}

// "(LINE)" или "(LINE,COL)" — колонки нет у диагностик, привязанных к строке целиком.
bool parse_location(const std::string& inside, int& line, int& col) {
    const size_t comma = inside.find(',');
    if (comma == std::string::npos) {
        if (!all_digits(inside)) return false;
        line = std::atoi(inside.c_str());
        col = 0;
        return true;
    }
    const std::string l = inside.substr(0, comma);
    const std::string c = inside.substr(comma + 1);
    if (!all_digits(l) || !all_digits(c)) return false;
    line = std::atoi(l.c_str());
    col = std::atoi(c.c_str());
    return true;
}

// Код диагностики — только строгая форма «буквы + цифры» (C2065, LNK1104, D9025). Иначе `error
// while processing module: bad file` отдало бы кодом кусок фразы, а хвост сообщения потерялся бы.
bool is_code(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && s[i] >= 'A' && s[i] <= 'Z') ++i;
    if (i == 0 || i == s.size()) return false;
    return all_digits(s.substr(i));
}

// "fatal error" сводится к "error", а не заводит четвёртую степень: панель редактора красит по
// severity, и «фатальность» ничего не меняет ни для click-to-open, ни для цвета строки.
struct Severity {
    const char* text;
    const char* normalized;
};

bool parse_severity(const std::string& rest, size_t& after, std::string& sev) {
    static const Severity kinds[] = {
        {"fatal error", "error"}, {"error", "error"}, {"warning", "warning"}, {"note", "note"}};
    for (const Severity& k : kinds) {
        const size_t n = std::string(k.text).size();
        if (rest.compare(0, n, k.text) != 0) continue;
        // Дальше обязан идти разделитель: иначе "errors" или "warning_count" сойдут за диагностику.
        if (rest.size() <= n || (rest[n] != ' ' && rest[n] != ':')) continue;
        after = n;
        sev = k.normalized;
        return true;
    }
    return false;
}

} // namespace

// MSVC/clang-cl: `path\file.cpp(12,5): error C2065: 'foo': undeclared identifier`.
// Отличия от gnu-формата, из-за которых общий разбор невозможен: локация в скобках (двоеточие
// после пути занято буквой диска), колонка необязательна, а между severity и сообщением стоит
// код диагностики — «C2065» не часть текста ошибки и не должен уезжать в сообщение.
bool parse_msvc_line(const std::string& line, Diagnostic& out) {
    // Кандидатов на локацию перебираем слева направо: и путь (`C:\a (x): b\f.cpp(1,2): error …`),
    // и сообщение вправе содержать «): », поэтому ни первое, ни последнее вхождение само по себе
    // не является локацией — ею является первое, из которого разбираются номера.
    Diagnostic d;
    size_t close = 0;
    size_t open = std::string::npos;
    for (size_t at = line.find("): "); at != std::string::npos; at = line.find("): ", at + 1)) {
        if (at == 0) continue;
        const size_t o = line.rfind('(', at);
        if (o == std::string::npos || o == 0) continue;
        if (!parse_location(line.substr(o + 1, at - o - 1), d.line, d.col)) continue;
        close = at;
        open = o;
        break;
    }
    if (open == std::string::npos) return false;
    d.file = line.substr(0, open);
    if (d.file.empty()) return false;

    const std::string rest = line.substr(close + 3);
    size_t after = 0;
    if (!parse_severity(rest, after, d.severity)) return false;

    // Между severity и сообщением — либо ": " сразу (clang-cl кода не печатает), либо " CODE: ".
    size_t msg = std::string::npos;
    if (rest[after] == ':') {
        msg = after + 1;
    } else {
        const size_t colon = rest.find(':', after);
        if (colon == std::string::npos) return false;
        const std::string code = rest.substr(after + 1, colon - after - 1);
        // Не код — значит двоеточие принадлежит сообщению, и резать по нему нечего.
        if (!is_code(code)) return false;
        d.code = code;
        msg = colon + 1;
    }
    while (msg < rest.size() && rest[msg] == ' ') ++msg;
    d.message = rest.substr(msg);
    out = std::move(d);
    return true;
}

} // namespace ide::build
