#pragma once
#include <span>
#include <string>

#include "assetc_manifest.hpp"
#include "manifest_bake.hpp"

// Бейк записи игрового вида (спека #29, В2а): пекарь вида отдаёт байты таблицы, секцию по имени
// записи собирает ядро. Текст файла прочитан проходом записей, клипы — всего манифеста.
namespace asset::manifest {

struct TextRecord {
    Record record;
    std::string file, text;
};

bool bake_kind(Bake& b, const TextRecord& f, std::span<const framework::graphics::ClipSrc> clips);

} // namespace asset::manifest
