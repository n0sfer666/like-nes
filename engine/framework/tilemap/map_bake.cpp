#include <cstring>

#include "map_bake.hpp"
#include "map_format.hpp"
#include "platform_fs.hpp"

// Сборка байтов таблицы: заголовок, записи карт, блоки флагов, блоб имён. Грамматика исходника — в
// `map_parse.cpp`.
namespace framework::tilemap {
namespace {

void put(std::vector<uint8_t>& out, const void* p, std::size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    out.insert(out.end(), b, b + n);
}

// Блок флагов выравнивается на 4 байта. Не ради скорости: читатель кладёт блок в сетку `memcpy`, а
// вот САМ отступ обязан быть предсказуем, иначе смещение следующей карты зависело бы от чётности
// ширины предыдущей — то есть от данных, а не от формата.
uint64_t align4(uint64_t v) { return (v + 3u) & ~3ull; }

} // namespace

bool bake_maps(const std::string& text, std::vector<uint8_t>& out, MapBakeError& err) {
    std::vector<ParsedMap> maps;
    if (!parse_maps(text, maps, err)) return false;

    std::vector<char> blob{'\0'};   // смещение 0 — пустая строка, значит «имени нет»
    std::vector<MapRow> rows;
    rows.reserve(maps.size());
    // Дедупа имён здесь нет намеренно: повтор имени карты отвергает `parse_maps`, и ветка «имя уже
    // в блобе» была бы кодом, который не может выполниться — в пути, который печёт байты голдена.
    uint64_t cursor = sizeof(MapHeader) + maps.size() * sizeof(MapRow);
    for (const ParsedMap& m : maps) {
        const auto off = static_cast<uint32_t>(blob.size());
        blob.insert(blob.end(), m.name.begin(), m.name.end());
        blob.push_back('\0');
        MapRow r{};
        r.name_offset = off;
        r.origin_x_raw = m.origin.x.raw;
        r.origin_y_raw = m.origin.y.raw;
        r.tile_size_raw = m.tile_size.raw;
        r.width = m.width;
        r.height = m.height;
        r.tiles_offset = static_cast<uint32_t>(cursor);
        rows.push_back(r);
        cursor = align4(cursor + m.flags.size() * sizeof(TileFlags));
        if (cursor > 0xFFFFFFFFull) {
            err.line = 0;
            err.message = "the table does not fit the 32-bit offsets of the format";
            return false;
        }
    }

    MapHeader h{};
    std::memcpy(h.magic, MAP_MAGIC, sizeof(h.magic));
    h.version = MAP_VERSION;
    h.map_count = static_cast<uint32_t>(rows.size());
    h.maps_offset = static_cast<uint32_t>(sizeof(MapHeader));
    h.strings_offset = static_cast<uint32_t>(cursor);
    const uint64_t total = cursor + blob.size();
    if (total > 0xFFFFFFFFull) {
        err.line = 0;
        err.message = "the table does not fit the 32-bit offsets of the format";
        return false;
    }
    h.total_size = static_cast<uint32_t>(total);

    out.clear();
    out.reserve(h.total_size);
    put(out, &h, sizeof(h));
    for (const MapRow& r : rows) put(out, &r, sizeof(r));
    for (std::size_t i = 0; i < maps.size(); ++i) {
        out.resize(rows[i].tiles_offset, 0);   // отступ выравнивания — нулями, а не мусором стека
        put(out, maps[i].flags.data(), maps[i].flags.size() * sizeof(TileFlags));
    }
    out.resize(h.strings_offset, 0);
    put(out, blob.data(), blob.size());
    return true;
}

namespace {

bool read_source(const std::string& path, std::string& text, MapBakeError& err) {
    if (platform::read_text(path, text)) return true;
    err.line = 0;
    err.message = "cannot read " + path;
    return false;
}

} // namespace

bool bake_maps_file(const std::string& path, std::vector<uint8_t>& out, MapBakeError& err) {
    std::string text;
    if (!read_source(path, text, err)) return false;
    return bake_maps(text, out, err);
}

// Разбор файла без сборки байтов: гейт живого бандла сверяет сетку с ИСХОДНИКОМ, и таблица ему для
// этого не нужна — а перепечь её ради сверки значило бы сверять бандл с бандлом.
bool parse_maps_file(const std::string& path, std::vector<ParsedMap>& out, MapBakeError& err) {
    std::string text;
    if (!read_source(path, text, err)) return false;
    return parse_maps(text, out, err);
}

} // namespace framework::tilemap
