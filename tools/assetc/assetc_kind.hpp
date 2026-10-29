#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "clip_bake.hpp"

// Игровой вид записи манифеста (спека #29, В2а): `<вид>|<имя>|<путь><extension>`. Вид приносит
// инструмент игры, ядро бейка его не знает; пекарь отдаёт байты таблицы, секцию собирает ядро.
namespace asset::manifest {

struct KindError {
    uint32_t line = 0;
    std::string message;
};

struct Kind {
    const char* name;
    const char* extension;
    bool (*bake)(const char* name, const std::string& text, std::span<const framework::graphics::ClipSrc> clips,
                 std::vector<uint8_t>& table, KindError& error);
};

} // namespace asset::manifest
