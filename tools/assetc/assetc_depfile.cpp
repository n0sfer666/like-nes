#include "assetc_depfile.hpp"

namespace asset::depfile {
namespace {

void append(std::string& out, const std::string& path, bool backslash_is_sep) {
    for (char c : path) {
        if (c == '\\' && backslash_is_sep) out += '/';
        else if (c == ' ' || c == '#') (out += '\\') += c;
        else if (c == '$') out += "$$";
        else out += c;
    }
}

} // namespace

std::string text(const std::string& target, const std::vector<std::string>& deps,
                 bool backslash_is_sep) {
    std::string out;
    append(out, target, backslash_is_sep);
    out += ':';
    for (const std::string& d : deps) {
        out += ' ';
        append(out, d, backslash_is_sep);
    }
    out += '\n';
    return out;
}

} // namespace asset::depfile
