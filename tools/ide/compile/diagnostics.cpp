#include "diagnostics.hpp"
#include <cstdlib>
#include <sstream>

namespace ide::build {
namespace {

// Ищет " <sev>: " и возвращает индекс начала маркера + длину, если это error/warning/note.
bool find_severity(const std::string& s, size_t& pos, std::string& sev) {
    static const char* kinds[] = {"error", "warning", "note"};
    for (const char* k : kinds) {
        std::string marker = std::string(": ") + k + ": ";
        size_t p = s.find(marker);
        if (p != std::string::npos) { pos = p; sev = k; return true; }
    }
    return false;
}

// prefix = "path:line:col" → распарсить с правого конца (путь может содержать ':' редко, но
// line/col — последние два числовых поля).
bool parse_prefix(const std::string& prefix, std::string& file, int& line, int& col) {
    size_t c2 = prefix.rfind(':');
    if (c2 == std::string::npos || c2 == 0) return false;
    size_t c1 = prefix.rfind(':', c2 - 1);
    if (c1 == std::string::npos) return false;
    std::string col_s = prefix.substr(c2 + 1);
    std::string line_s = prefix.substr(c1 + 1, c2 - c1 - 1);
    if (col_s.empty() || line_s.empty()) return false;
    for (char ch : col_s) if (ch < '0' || ch > '9') return false;
    for (char ch : line_s) if (ch < '0' || ch > '9') return false;
    file = prefix.substr(0, c1);
    line = std::atoi(line_s.c_str());
    col = std::atoi(col_s.c_str());
    return !file.empty();
}

} // namespace

std::vector<Diagnostic> parse_diagnostics(const std::string& compiler_output) {
    std::vector<Diagnostic> out;
    std::istringstream in(compiler_output);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t pos = 0;
        std::string sev;
        if (!find_severity(line, pos, sev)) continue;
        std::string prefix = line.substr(0, pos);
        Diagnostic d;
        if (!parse_prefix(prefix, d.file, d.line, d.col)) continue;
        d.severity = sev;
        d.message = line.substr(pos + sev.size() + 4);  // ": " + sev + ": "
        out.push_back(std::move(d));
    }
    return out;
}

} // namespace ide::build
