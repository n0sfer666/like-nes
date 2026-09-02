#include "bakers.hpp"

#include <cstdio>

#include "../../engine/achievements/bake.hpp"
#include "../../engine/framework/character/profile_bake.hpp"
#include "../../engine/framework/graphics/atlas_bake.hpp"
#include "../../engine/framework/input/preset_bake.hpp"
#include "../../engine/framework/tilemap/map_bake.hpp"
#include "../../engine/material/bake.hpp"
#include "baker_guid.hpp"
#include "format.hpp"
#include "platform_fs.hpp"

// Пекари zero-parse таблиц: текстовый исходник → байты, которые рантайм читает прямо из
// mmap-региона, без парсинга. От соседей по `bakers.cpp` они отличаются тем, что внешних кодеков не
// зовут вовсе, — и ровно поэтому именно эти секции сверяются `--verify-game` на трёх ОС.
namespace asset::bakers {
namespace {

// Сборка ассета одна на всех: тип, кодек и резидентность у zero-parse таблицы не выбираются, а
// следуют из самого шва. Пекари различаются ИСХОДНИКОМ и именем guid — и только ими.
void push_table(const char* name, std::vector<uint8_t>&& table, std::vector<AssetInput>& out) {
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Raw;
    a.codec = Codec::Raw;
    a.residency = Residency::Mmap;
    a.uncompressed_size = static_cast<uint32_t>(table.size());
    a.payload = std::move(table);
    out.push_back(std::move(a));
}

// Отказ печатается ОДИНАКОВО для всех: имя секции, файл, номер строки, причина словами.
// Номер строки тут несущий — он единственное, что отличает «исходник не разобрался» от «а где».
void report(const char* section, const std::string& src, int line, const std::string& message) {
    std::fprintf(stderr, "[assetc] %s %s: line %d: %s\n", section, src.c_str(), line,
                 message.c_str());
}

} // namespace

// Достижения (спека #10): текстовый исходник → zero-parse таблица в бандле.
// Рантайм читает её из mmap-региона без парсинга; guid = fnv1a("achievements").
bool achievements(const std::string& src, std::vector<AssetInput>& out) {
    std::vector<uint8_t> table;
    ach::BakeError err;
    if (!ach::bake_manifest_file(src, table, err)) {
        report("achievements", src, err.line, err.message);
        return false;
    }
    push_table("achievements", std::move(table), out);
    return true;
}

// Пресеты ввода (спека #14): текстовый манифест → zero-parse таблица в бандле, как достижения.
// Раскладка — данные, поэтому правка биндинга не требует пересборки игры.
bool input_presets(const std::string& src, std::vector<AssetInput>& out) {
    std::vector<uint8_t> table;
    framework::input::PresetBakeError err;
    if (!framework::input::bake_presets_file(src, table, err)) {
        report("input", src, err.line, err.message);
        return false;
    }
    push_table("input", std::move(table), out);
    return true;
}

// Профиль движения (спека #16): тот же zero-parse шов, что у пресетов. Настройка ОЩУЩЕНИЯ правится
// десятками итераций подряд, и пересборка движка на каждую правку высоты прыжка убивает сам цикл
// подбора — поэтому профиль едет данными; guid = fnv1a("movement").
bool movement(const std::string& src, std::vector<AssetInput>& out) {
    std::vector<uint8_t> table;
    framework::character::ProfileBakeError err;
    if (!framework::character::bake_profiles_file(src, table, err)) {
        report("movement", src, err.line, err.message);
        return false;
    }
    push_table("movement", std::move(table), out);
    return true;
}

// Тайловая карта (спека #16, вертикаль 2): тот же zero-parse шов, что у профиля. Раскладка уровня
// правится десятками итераций подряд, и пересборка движка на каждый передвинутый тайл убивает сам
// цикл подбора — поэтому карта едет данными; guid = fnv1a("tilemap").
bool tilemap(const std::string& src, std::vector<AssetInput>& out) {
    std::vector<uint8_t> table;
    framework::tilemap::MapBakeError err;
    if (!framework::tilemap::bake_maps_file(src, table, err)) {
        report("tilemap", src, err.line, err.message);
        return false;
    }
    push_table("tilemap", std::move(table), out);
    return true;
}

// Нарезка атласа (спека #17, вертикаль 1, шаг D): тот же zero-parse шов, что у карты. Имя guid —
// `atlas_regions`, а не `atlas`: `atlas` уже занят САМОЙ ТЕКСТУРОЙ игры-образца, и совпадение guid
// означало бы, что вторая запись бандла молча вытесняет первую. Нарезка правится каждым
// перерисованным спрайтом, и пересборка движка на сдвинутый на пиксель кадр убивает цикл рисования.
bool atlas_regions(const std::string& src, std::vector<AssetInput>& out) {
    std::vector<uint8_t> table;
    framework::graphics::AtlasBakeError err;
    if (!framework::graphics::bake_atlas_file(src, table, err)) {
        report("atlas", src, err.line, err.message);
        return false;
    }
    push_table("atlas_regions", std::move(table), out);
    return true;
}

// Материалы (спека #18): тот же zero-parse шов, что у нарезки атласа. Пекарь здесь — ЧИСТЫЙ
// парсер: ни tint, ни basisu он не зовёт, поэтому секция печётся на любой машине и попадает в тот
// же класс, что сверяет `--verify-game`. Шейдеры библиотеки едут отдельными ассетами через
// `bakers::shader` — материал ссылается на них guid'ом имени, а не содержимым.
bool materials(const std::string& src, const std::string& wgsl_src,
               std::vector<AssetInput>& out) {
    std::vector<uint8_t> table;
    mat::BakeError err;
    if (!mat::bake_materials_file(src, table, err)) {
        report("materials", src, err.line, err.message);
        return false;
    }
    push_table("materials", std::move(table), out);

    // Текст модуля едет ТЕМ ЖЕ бандлом, отдельным ассетом. Иначе рантайм искал бы исходник рядом с
    // exe: у таблицы материалов есть имена точек входа и нет модуля, в котором их искать, а
    // библиотека, чей шейдер лежит вне бандла, работает ровно до первой установленной сборки.
    std::vector<uint8_t> wgsl;
    if (!platform::read_bytes(wgsl_src, wgsl) || wgsl.empty()) {
        report("materials", wgsl_src, 0, "shader module unreadable");
        return false;
    }
    wgsl.push_back(0);   // модуль уезжает в wgpu строкой: терминатор кладёт пекарь, а не читатель
    push_table("effects.wgsl", std::move(wgsl), out);
    return true;
}

} // namespace asset::bakers
