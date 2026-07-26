#include "platform_path.hpp"

namespace platform {

bool is_sep(char c) { return c == '/' || c == '\\'; }

bool is_absolute(const std::string& path) {
    if (path.size() >= 2 && is_sep(path[0]) && is_sep(path[1])) return true;
    const char d = path.empty() ? '\0' : path[0];
    // "C:dir" — не абсолютный: у каждого диска свой текущий каталог, разрешает его CWD процесса.
    return path.size() >= 3 && path[1] == ':' && is_sep(path[2]) &&
           ((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z'));
}

size_t root_len(const std::string& path) {
    if (path.size() >= 2 && is_sep(path[0]) && is_sep(path[1])) {
        size_t i = 2;
        // Сегмент — непустой: дубль разделителя между сервером и шарой иначе обрывал бы корень
        // внутри имени шары, и mkdir по нему вернул бы ACCESS_DENIED.
        for (int seg = 0; seg < 2 && i < path.size(); ++seg) {
            while (i < path.size() && is_sep(path[i])) ++i;
            while (i < path.size() && !is_sep(path[i])) ++i;
        }
        while (i < path.size() && is_sep(path[i])) ++i;
        return i;
    }
    const char d = path.empty() ? '\0' : path[0];
    const bool drive = path.size() >= 2 && path[1] == ':' &&
                       ((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z'));
    if (drive) return path.size() >= 3 && is_sep(path[2]) ? 3 : 2;
    return path.empty() || !is_sep(path[0]) ? 0 : 1;
}

} // namespace platform
