#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace music_ogg_corruption {
// Committed stereo48000 fixture: портим третий audio packet после валидных initial packets.
// Ogg CRC исправляется; контейнер/declared4800frames открываются, payload даёт shortread позже.
inline bool apply(std::vector<uint8_t>& bytes) {
    size_t page = 0;
    for (uint32_t index = 0; index < 3; ++index) {
        if (page + 27 > bytes.size()) return false;
        const size_t body = page + 27 + bytes[page + 26];
        if (body > bytes.size()) return false;
        size_t end = body;
        for (size_t i = page + 27; i < body; ++i) end += bytes[i];
        if (end > bytes.size()) return false;
        if (index != 2) { page = end; continue; }
        size_t packet_start = body, cursor = body; uint32_t packet = 0; bool changed = false;
        for (size_t i = page + 27; i < body; ++i) {
            cursor += bytes[i];
            if (bytes[i] == 255) continue;
            if (packet == 2 && packet_start < cursor) { bytes[packet_start] |= 1; changed = true; break; }
            ++packet; packet_start = cursor;
        }
        if (!changed) return false;
        std::fill_n(bytes.data() + page + 22, 4, 0);
        uint32_t crc = 0;
        for (size_t i = page; i < end; ++i) {
            crc ^= uint32_t{bytes[i]} << 24;
            for (uint32_t bit = 0; bit < 8; ++bit) crc = (crc << 1) ^ ((crc & 0x80000000u) ? 0x04c11db7u : 0u);
        }
        for (uint32_t i = 0; i < 4; ++i) bytes[page + 22 + i] = static_cast<uint8_t>(crc >> (i * 8));
        return true;
    }
    return false;
}
}
