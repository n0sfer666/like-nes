#include <cstring>

#include "atlas_bake.hpp"
#include "atlas_format.hpp"
#include "platform_fs.hpp"

// Сборка байтов таблицы: заголовок, записи регионов, блоб имён. Грамматика исходника — в
// `atlas_parse.cpp`.
namespace framework::graphics {
namespace {

void put(std::vector<uint8_t>& out, const void* p, std::size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    out.insert(out.end(), b, b + n);
}

} // namespace

bool bake_atlas(const std::string& text, std::vector<uint8_t>& out, AtlasBakeError& err) {
    ParsedAtlas atlas;
    if (!parse_atlas(text, atlas, err)) return false;

    std::vector<char> blob{'\0'};   // смещение 0 — пустая строка, значит «имени нет»
    std::vector<AtlasRow> rows;
    rows.reserve(atlas.regions.size());
    // Дедупа имён здесь нет намеренно: повтор имени отвергает `parse_atlas`, и ветка «имя уже в
    // блобе» была бы кодом, который не может выполниться — в пути, который печёт байты голдена.
    for (const ParsedRegion& r : atlas.regions) {
        AtlasRow row{};
        row.name_offset = static_cast<uint32_t>(blob.size());
        blob.insert(blob.end(), r.name.begin(), r.name.end());
        blob.push_back('\0');
        row.x = r.x;
        row.y = r.y;
        row.w = r.w;
        row.h = r.h;
        row.pivot_x_raw = r.pivot.x.raw;
        row.pivot_y_raw = r.pivot.y.raw;
        rows.push_back(row);
    }

    // Отступа выравнивания между записями и блобом нет и быть не может: `AtlasRow` кратен четырём
    // по своему static_assert'у, а блоб имён — поток байтов, у которого требований к выравниванию
    // нет вовсе. Поэтому и «дыры», зависящей от данных, тут не заводится (у карт #16 она была: там
    // между записями лежат блоки флагов переменной длины).
    const uint64_t strings_offset = sizeof(AtlasHeader) + rows.size() * sizeof(AtlasRow);
    const uint64_t total = strings_offset + blob.size();
    if (total > 0xFFFFFFFFull) {
        err.line = 0;
        err.message = "the table does not fit the 32-bit offsets of the format";
        return false;
    }

    AtlasHeader h{};
    std::memcpy(h.magic, ATLAS_MAGIC, sizeof(h.magic));
    h.version = ATLAS_VERSION;
    h.region_count = static_cast<uint32_t>(rows.size());
    h.regions_offset = static_cast<uint32_t>(sizeof(AtlasHeader));
    h.strings_offset = static_cast<uint32_t>(strings_offset);
    h.total_size = static_cast<uint32_t>(total);
    h.page_width = atlas.page_width;
    h.page_height = atlas.page_height;

    out.clear();
    out.reserve(h.total_size);
    put(out, &h, sizeof(h));
    for (const AtlasRow& r : rows) put(out, &r, sizeof(r));
    put(out, blob.data(), blob.size());
    return true;
}

namespace {

bool read_source(const std::string& path, std::string& text, AtlasBakeError& err) {
    if (platform::read_text(path, text)) return true;
    err.line = 0;
    err.message = "cannot read " + path;
    return false;
}

} // namespace

bool bake_atlas_file(const std::string& path, std::vector<uint8_t>& out, AtlasBakeError& err) {
    std::string text;
    if (!read_source(path, text, err)) return false;
    return bake_atlas(text, out, err);
}

// Разбор файла без сборки байтов: гейт живого бандла сверяет таблицу с ИСХОДНИКОМ, и перепекать её
// ради сверки значило бы сверять бандл с бандлом.
bool parse_atlas_file(const std::string& path, ParsedAtlas& out, AtlasBakeError& err) {
    std::string text;
    if (!read_source(path, text, err)) return false;
    return parse_atlas(text, out, err);
}

} // namespace framework::graphics
