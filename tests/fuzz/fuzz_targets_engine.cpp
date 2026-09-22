// Цели гейта 9 из движка: конверт бандла (LNAB) и таблица материалов (LNMT). Разбиты по
// ЗАВИСИМОСТЯМ, а не по числу строк: эта пара линкуется с `asset_core` и `material_*`, достижения
// уехали в `fuzz_targets_ach.cpp` к своему `ach_core`, а держать всё это в одном файле с целями
// framework'а значило бы тянуть половину дерева в каждую единицу трансляции.
#include <cstring>
#include <vector>

// Пути КАТАЛОГОМ, а не одним именем: в дереве по два `bake.hpp`, `manifest.hpp`, `registry.hpp` и
// `state.hpp` — у материалов и достижений, у достижений и плагинов, у достижений и персонажа.
// Короткое имя выбирал бы порядок каталогов в линковке, то есть случай.
#include "asset/bundle_view.hpp"
#include "asset/bundle_writer.hpp"
#include "fuzz_target.hpp"
#include "material/bake.hpp"
#include "material/table.hpp"

namespace fuzz {
namespace {

// Биты float'а складываются ЧЕРЕЗ memcpy, а не приведением к целому: мутация вправе положить в
// поле NaN или 1e30, и `static_cast<uint64_t>` на таком значении — само по себе UB, то есть гейт
// падал бы на своей же обвязке и обвинял читателя.
void consume_float(float f) {
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    consume(bits);
}

// --- конверт бандла (LNAB) ---

std::vector<uint8_t> seed_bundle() {
    asset::AssetInput raw{};
    raw.guid = 0x1111111111111111ull;
    raw.type = asset::AssetType::Raw;
    raw.codec = asset::Codec::Raw;
    raw.residency = asset::Residency::Mmap;
    raw.payload = {1, 2, 3, 4, 5, 6, 7, 8};
    raw.uncompressed_size = 8;

    // Вторая запись — ТЕКСТУРНАЯ: у неё заполнены tex_*/variant_key, и без неё мутации по этим
    // полям приезжали бы в нули, которые читатель и так отвергает раньше по типу.
    asset::AssetInput tex{};
    tex.guid = 0x2222222222222222ull;
    tex.type = asset::AssetType::Texture;
    tex.codec = asset::Codec::Ktx2Uastc;
    tex.residency = asset::Residency::Stream;
    tex.payload = {9, 10, 11, 12};
    tex.uncompressed_size = 64;
    tex.tex_w = 4;
    tex.tex_h = 4;
    tex.tex_format = 1;
    tex.variant_key = 7;

    return asset::write_bundle({raw, tex});
}

bool read_bundle(const uint8_t* data, size_t size) {
    asset::BundleView view;
    // `trusted=false` — тот самый путь, которым в движок приезжает чужой файл; на доверенном
    // читатель проверок и не обещает, и гейт проверял бы не тот контракт.
    if (!view.open(data, size, false)) return false;
    for (uint32_t i = 0; i < view.count(); ++i) {
        const asset::AssetEntry& e = view.entry(i);
        consume_all(e.guid, e.payload_size, e.uncompressed_size, e.tex_w, e.tex_h);
        const uint8_t* p = view.payload(e);
        if (p != nullptr) {
            for (uint32_t b = 0; b < e.payload_size; ++b) consume(p[b]);
        }
        consume(view.find(e.guid) != nullptr ? 1u : 0u);
    }
    return true;
}

// --- таблица материалов (LNMT) ---

const char* const MATERIAL_SRC =
    "material | a | s | alpha\n"
    "param | p | scalar | raw | 0\n"
    "tex | n | noise | 0\n"
    "instance | i | a\n"
    "set | p | 1\n";

std::vector<uint8_t> seed_materials() {
    std::vector<uint8_t> bytes;
    mat::BakeError err;
    mat::bake_materials(MATERIAL_SRC, bytes, err);
    return bytes;
}

bool read_materials(const uint8_t* data, size_t size) {
    mat::Table t;
    if (t.load(data, size) != mat::LoadResult::Ok) return false;
    for (uint32_t i = 0; i < t.count(); ++i) {
        const mat::MaterialRow& r = t.row(i);
        consume_all(r.shader_guid, r.base, r.blend);
        consume_str(t.name(r.name_off));
        consume_str(t.shader(i));
        for (uint16_t k = 0; k < r.param_count; ++k) {
            const mat::ParamRow& p = t.param(r.param_first + k);
            consume_all(p.type, p.unit, p.slot);
            consume_str(t.name(p.name_off));
            for (float v : p.value) consume_float(v);
        }
        for (uint16_t k = 0; k < r.texture_count; ++k) {
            const mat::TextureRow& x = t.texture(r.texture_first + k);
            consume_all(x.guid, x.binding);
            consume_str(t.name(x.name_off));
        }
        // Разрешение наследования — отдельный проход по цепочке `base`, и без него мутация по
        // `base` уходила бы в поле, которое никто не разыменовывает.
        float block[mat::PARAM_BLOCK_FLOATS] = {};
        t.resolve(i, block);
        for (float v : block) consume_float(v);
        consume(t.find(t.name(r.name_off)));
    }
    return true;
}

const Target TARGETS[] = {
    {"bundle", seed_bundle, read_bundle},
    {"material-table", seed_materials, read_materials},
};

} // namespace

const Target* engine_targets(std::size_t* count) {
    *count = sizeof(TARGETS) / sizeof(TARGETS[0]);
    return TARGETS;
}

} // namespace fuzz
