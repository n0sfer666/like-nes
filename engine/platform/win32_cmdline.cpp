#include "win32_cmdline.hpp"

namespace platform::win32 {
namespace {

// Правила CommandLineToArgvW: кавычка экранируется обратным слэшем, а слэши ПЕРЕД кавычкой
// удваиваются. Наивная склейка через пробел ломается на первом же `C:\Program Files\...`.
void append_quoted(std::wstring& out, const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        out += arg;
        return;
    }
    out += L'"';
    for (size_t i = 0;; ++i) {
        size_t slashes = 0;
        while (i < arg.size() && arg[i] == L'\\') {
            ++i;
            ++slashes;
        }
        if (i == arg.size()) {
            out.append(slashes * 2, L'\\'); // закрывающую кавычку экранировать нельзя
            break;
        }
        if (arg[i] == L'"') {
            out.append(slashes * 2 + 1, L'\\');
        } else {
            out.append(slashes, L'\\');
        }
        out += arg[i];
    }
    out += L'"';
}

} // namespace

std::wstring command_line(const std::vector<std::wstring>& argv) {
    std::wstring out;
    for (const std::wstring& a : argv) {
        if (!out.empty()) out += L' ';
        append_quoted(out, a);
    }
    return out;
}

} // namespace platform::win32
