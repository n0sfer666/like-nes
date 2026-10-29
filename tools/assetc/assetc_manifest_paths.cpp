#include "assetc_manifest.hpp"

#include "platform_fs.hpp"

namespace asset::manifest {
namespace {

bool same_ignoring_case(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

} // namespace

bool name_ok(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

// `..` отбивается целиком, а не только выводящий наружу: запись с ним всегда можно написать
// прямо, а лексическая проверка «внутри ли» — второе правило, в котором можно ошибиться. Ссылки
// не проверяются: манифест пишет автор игры о своём дереве, общие ассеты через ссылку — законный
// приём, и правило держит переносимость записи, а не песочницу. `:` — диск или поток NTFS,
// `\` — разделитель, который POSIX считает частью имени.
bool path_ok(const std::string& path, std::string& error) {
    if (path.empty()) {
        error = "empty path";
        return false;
    }
    if (path.find('\\') != std::string::npos) {
        error = "path '" + path + "' uses '\\'; write paths with '/'";
        return false;
    }
    if (path[0] == '/' || (path.size() > 1 && path[1] == ':')) {
        error = "path '" + path + "' is absolute; paths are relative to the manifest";
        return false;
    }
    if (path.find(':') != std::string::npos) {
        error = "path '" + path + "' uses ':'; Windows reads it as a drive or a stream";
        return false;
    }
    size_t start = 0;
    while (start <= path.size()) {
        const size_t slash = path.find('/', start);
        const size_t end = slash == std::string::npos ? path.size() : slash;
        const std::string seg = path.substr(start, end - start);
        if (seg == "..") {
            error = "path '" + path + "' uses '..'; files must lie under the manifest directory";
            return false;
        }
        if (seg.empty() || seg == ".") {
            error = "path '" + path + "' has an empty or '.' segment";
            return false;
        }
        start = end + 1;
    }
    return true;
}

// Путь лексически проверен `parse`: сегменты непустые, без `.` и `..`, разделитель `/`.
bool resolve(const std::string& base_dir, const std::string& rel, std::string& full,
             std::string& error) {
    std::string dir = base_dir;
    std::vector<std::string> names;
    size_t start = 0;
    while (start < rel.size()) {
        const size_t slash = rel.find('/', start);
        const size_t end = slash == std::string::npos ? rel.size() : slash;
        const std::string seg = rel.substr(start, end - start);
        if (!platform::list_dir(dir, names)) {
            error = "cannot list directory " + dir;
            return false;
        }
        bool exact = false;
        std::string near;
        for (const std::string& n : names) {
            if (n == seg) exact = true;
            else if (same_ignoring_case(n, seg)) near = n;
        }
        if (!exact) {
            error = near.empty() ? "'" + rel + "' not found: no '" + seg + "' in " + dir
                                 : "'" + rel + "': '" + seg + "' is spelled '" + near +
                                       "' on disk; match the case exactly";
            return false;
        }
        dir += "/" + seg;
        start = end + 1;
    }
    if (platform::is_dir(dir)) {
        error = "'" + rel + "' is a directory, not a file";
        return false;
    }
    full = dir;
    return true;
}

} // namespace asset::manifest
