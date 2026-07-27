#include "platform_fs.hpp"

#include "platform_path.hpp"

namespace platform {

std::string exe_dir() {
    const std::string p = exe_path();
    if (p.empty()) return {};
    size_t slash = p.size();
    while (slash > 0 && !is_sep(p[slash - 1])) --slash;
    if (slash == 0) return std::string(".");
    // Обрезать разделитель можно не всегда: у exe в корне (`C:\app.exe`, `/app`) отрезанный слэш
    // оставил бы `C:` — drive-relative путь, который разрешается от текущего каталога диска, — и
    // `exe_dir() + "/assets"` уехал бы мимо. В корне возвращаем сам корень.
    const size_t root = root_len(p);
    return slash <= root ? p.substr(0, root) : p.substr(0, slash - 1);
}

bool read_text(const std::string& path, std::string& out) {
    std::FILE* f = open_file(path, "rb");
    if (f == nullptr) return false;
    out.clear();
    char buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;) out.append(buf, n);
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) out.clear();
    return ok;
}

bool read_bytes(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = open_file(path, "rb");
    if (f == nullptr) return false;
    out.clear();
    uint8_t buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;) out.insert(out.end(), buf, buf + n);
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) out.clear();
    return ok;
}

bool ensure_dir(const std::string& path) {
    const size_t root = root_len(path);
    size_t end = path.size();
    while (end > root && is_sep(path[end - 1])) --end;
    for (size_t i = root; i < end; ++i) {
        // Пустой компонент (дубль разделителя) каталогом не является: на "C:\\dir" префикс до
        // второго слэша — сам корень, и mkdir по нему обрубил бы цепочку с ACCESS_DENIED.
        if (!is_sep(path[i]) || is_sep(path[i - 1])) continue;
        if (!make_dir(path.substr(0, i))) return false;
    }
    // За корнем ничего нет: CreateDirectoryW("D:\\") даёт ACCESS_DENIED, а не ALREADY_EXISTS, и
    // явно заданный каталог сейвов молча уехал бы в фолбэк.
    if (end <= root) return is_dir(path);
    return make_dir(path.substr(0, end));
}

} // namespace platform
