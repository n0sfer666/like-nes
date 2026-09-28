#pragma once
#include <cstddef>
#include <string>

namespace platform {

// Общая для двух реализаций run_capture граница потолка: false — кусок в потолок не влез, в out
// дописано сколько влезло. Своя копия у каждой ОС разошлась бы на «ровно max байт».
inline bool append_capped(std::string& out, const char* data, size_t n, size_t max) {
    const size_t room = out.size() < max ? max - out.size() : 0;
    if (n <= room) {
        out.append(data, n);
        return true;
    }
    out.append(data, room);
    return false;
}

} // namespace platform
