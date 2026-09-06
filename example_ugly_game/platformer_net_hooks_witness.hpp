#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "platform_fs.hpp"

// Улика гейта 9 (спека #22): сколько раз шов СПРОСИЛИ про ввод, сколько раз он ответил «ещё не
// готов» и сколько кадров у него попросили показать. Пишет её заглушка после прогона, читает
// утверждение в другом процессе — то есть формат общий, и живёт он одним местом, а не двумя
// копиями по разные стороны `platform::Child`.
//
// Нужна она потому, что по одному коду возврата половину гейта не доказать: код 10 верен и для
// пира, доигравшего прогон до конца и отказавшего на последнем кадре, а режим «шов отвечает через
// раз» неотличим от режима «шов всегда готов», пока прогон сходится с эталоном.
namespace platformer::fake {

struct Witness {
    uint32_t asked = 0;
    uint32_t refused = 0;
    uint32_t shown = 0;
};

inline std::string witness_path(const std::string& prefix, bool sender) {
    return prefix + (sender ? "-send" : "-recv") + ".hooks";
}

inline bool write_witness(const std::string& prefix, bool sender, const Witness& w) {
    std::FILE* f = platform::open_file(witness_path(prefix, sender), "wb");
    if (f == nullptr) return false;
    std::fprintf(f, "asked=%u refused=%u shown=%u\n", w.asked, w.refused, w.shown);
    return std::fclose(f) == 0;
}

// Своя цифра вместо `sscanf`: MSVC объявляет его небезопасным (C4996), а `/WX` делает из этого
// ошибку — то есть на единственной ОС, которую владелец проверить не может, файл просто не
// собирается. Тот же выбор и по той же причине, что в `platform_net.cpp`: разбор здесь на три поля
// фиксированного вида, и глушить диагностику `_CRT_SECURE_NO_WARNINGS` значило бы снимать её со
// ВСЕГО дерева ради одной строки.
inline bool read_field(const char*& p, const char* name, uint32_t& out) {
    const std::size_t n = std::strlen(name);
    while (*p == ' ') ++p;
    if (std::strncmp(p, name, n) != 0 || p[n] != '=') return false;
    p += n + 1;
    if (*p < '0' || *p > '9') return false;
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + static_cast<uint32_t>(*p++ - '0');
    out = v;
    return true;
}

inline bool read_witness(const std::string& prefix, bool sender, Witness& out) {
    std::string text;
    if (!platform::read_text(witness_path(prefix, sender), text)) return false;
    const char* p = text.c_str();
    return read_field(p, "asked", out.asked) && read_field(p, "refused", out.refused) &&
           read_field(p, "shown", out.shown);
}

// Улика убирается СВОЕЙ уборкой, а не вместе с файлами прогона (`runs::forget`): ту `spawn_pair`
// зовёт сразу после чтения результата, то есть снесла бы улику раньше, чем до неё дошло
// утверждение.
inline void forget_witness(const std::string& prefix) {
    platform::remove_file(witness_path(prefix, true));
    platform::remove_file(witness_path(prefix, false));
}

} // namespace platformer::fake
