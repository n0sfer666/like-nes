#include "bakers.hpp"

#include <cstdio>
#include <cstring>

#include "../../engine/achievements/bake.hpp"
#include "../../engine/framework/character/profile_bake.hpp"
#include "../../engine/framework/input/preset_bake.hpp"
#include "format.hpp"
#include "platform_fs.hpp"
#include "hash.hpp"

namespace asset::bakers {
namespace {

// GUID = FNV логического имени → стабилен (переживает rename файла), детерминирован cross-machine.
uint64_t guid_of(const char* name) { return fnv1a(name, std::strlen(name)); }

uint32_t bc7_size(uint32_t w, uint32_t h) {
    return ((w + 3) / 4) * ((h + 3) / 4) * 16; // 1 BC7-блок 4x4 = 16 байт
}

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    return platform::read_bytes(path, out);
}

// Синтетический ассет (raw payload детерм. паттерна) — без внешних кодеков.
AssetInput synth_asset(const char* name, AssetType type, Codec codec, Residency res, uint32_t size,
                       uint8_t seed) {
    std::vector<uint8_t> raw(size);
    for (uint32_t i = 0; i < size; ++i) raw[i] = static_cast<uint8_t>((i + seed) * 131u);
    AssetInput a;
    a.guid = guid_of(name);
    a.type = type; a.codec = codec; a.residency = res;
    a.uncompressed_size = size;
    a.payload = std::move(raw);
    return a;
}

} // namespace

bool texture(const codec::Tools& t, const std::string& src, const char* name, const std::string& tmp,
             std::vector<AssetInput>& out) {
    std::vector<uint8_t> ktx2;
    uint32_t w = 0, h = 0;
    if (!codec::png_to_ktx2(t, src, tmp, ktx2, w, h)) {
        std::fprintf(stderr, "[assetc] texture bake failed: %s\n", src.c_str());
        return false;
    }
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Texture;
    a.codec = Codec::Ktx2Uastc;
    a.residency = Residency::Stream;
    a.payload = std::move(ktx2);
    a.uncompressed_size = bc7_size(w, h); // размер BC7 в VRAM/арене
    a.tex_w = w;
    a.tex_h = h;
    a.tex_format = 0; // WGPUTextureFormat_BC7RGBAUnorm подставит рантайм
    out.push_back(std::move(a));
    return true;
}

bool shader(const codec::Tools& t, const std::string& src, const char* name, const std::string& ep,
            uint32_t stage, std::vector<AssetInput>& out) {
    std::vector<uint8_t> spv;
    if (!codec::wgsl_to_spirv(t, src, ep, spv)) {
        std::fprintf(stderr, "[assetc] shader bake failed: %s %s\n", src.c_str(), ep.c_str());
        return false;
    }
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Shader;
    a.codec = Codec::SpirV;
    a.residency = Residency::Mmap; // zero-copy в SPIRV-descriptor
    a.uncompressed_size = static_cast<uint32_t>(spv.size());
    a.tex_format = stage;   // для Shader: 0=vertex, 1=fragment (генерик meta-слот)
    a.variant_key = 0;      // on-demand variant-manifest: только фактические комбо
    a.payload = std::move(spv);
    out.push_back(std::move(a));
    return true;
}

// Аудио (спека #3): vorbis-контейнер как Mmap zero-copy (декод в рантайме stb_vorbis).
// Бейк детерминирован — копирует committed .ogg байты (энкодер не нужен в CI).
bool audio(const std::string& src, const char* name, bool loop, std::vector<AssetInput>& out) {
    std::vector<uint8_t> ogg;
    if (!read_file(src, ogg)) {
        std::fprintf(stderr, "[assetc] audio read failed: %s\n", src.c_str());
        return false;
    }
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Audio;
    a.codec = Codec::Vorbis;
    a.residency = Residency::Mmap; // zero-copy сжатые байты; PCM-декод — аудио-слой
    a.uncompressed_size = static_cast<uint32_t>(ogg.size());
    a.variant_key = loop ? AUDIO_FLAG_LOOP : 0u;
    a.payload = std::move(ogg);
    out.push_back(std::move(a));
    return true;
}

// Достижения (спека #10): текстовый исходник → zero-parse таблица в бандле.
// Рантайм читает её из mmap-региона без парсинга; guid = fnv1a("achievements").
bool achievements(const std::string& src, std::vector<AssetInput>& out) {
    std::vector<uint8_t> table;
    ach::BakeError err;
    if (!ach::bake_manifest_file(src, table, err)) {
        std::fprintf(stderr, "[assetc] achievements %s: line %d: %s\n", src.c_str(), err.line,
                     err.message.c_str());
        return false;
    }
    AssetInput a;
    a.guid = guid_of("achievements");
    a.type = AssetType::Raw;
    a.codec = Codec::Raw;
    a.residency = Residency::Mmap;
    a.uncompressed_size = static_cast<uint32_t>(table.size());
    a.payload = std::move(table);
    out.push_back(std::move(a));
    return true;
}

// Пресеты ввода (спека #14): текстовый манифест → zero-parse таблица в бандле, как достижения.
// Раскладка — данные, поэтому правка биндинга не требует пересборки игры.
bool input_presets(const std::string& src, std::vector<AssetInput>& out) {
    std::vector<uint8_t> table;
    framework::input::PresetBakeError err;
    if (!framework::input::bake_presets_file(src, table, err)) {
        std::fprintf(stderr, "[assetc] input %s: line %d: %s\n", src.c_str(), err.line,
                     err.message.c_str());
        return false;
    }
    AssetInput a;
    a.guid = guid_of("input");
    a.type = AssetType::Raw;
    a.codec = Codec::Raw;
    a.residency = Residency::Mmap;
    a.uncompressed_size = static_cast<uint32_t>(table.size());
    a.payload = std::move(table);
    out.push_back(std::move(a));
    return true;
}

// Профиль движения (спека #16): тот же zero-parse шов, что у пресетов. Настройка ОЩУЩЕНИЯ правится
// десятками итераций подряд, и пересборка движка на каждую правку высоты прыжка убивает сам цикл
// подбора — поэтому профиль едет данными; guid = fnv1a("movement").
bool movement(const std::string& src, std::vector<AssetInput>& out) {
    std::vector<uint8_t> table;
    framework::character::ProfileBakeError err;
    if (!framework::character::bake_profiles_file(src, table, err)) {
        std::fprintf(stderr, "[assetc] movement %s: line %d: %s\n", src.c_str(), err.line,
                     err.message.c_str());
        return false;
    }
    AssetInput a;
    a.guid = guid_of("movement");
    a.type = AssetType::Raw;
    a.codec = Codec::Raw;
    a.residency = Residency::Mmap;
    a.uncompressed_size = static_cast<uint32_t>(table.size());
    a.payload = std::move(table);
    out.push_back(std::move(a));
    return true;
}

void bulk(const char* name, std::vector<AssetInput>& out) {
    // Синтетический bulk-блок (меш/сцена): детерминированный паттерн → zstd → арена.
    std::vector<uint8_t> raw(64 * 1024);
    for (size_t i = 0; i < raw.size(); ++i)
        raw[i] = static_cast<uint8_t>((i * 2654435761u) >> 24);
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Bulk;
    a.codec = Codec::Zstd;
    a.residency = Residency::Stream;
    a.uncompressed_size = static_cast<uint32_t>(raw.size());
    a.payload = codec::zstd_compress(raw, 19); // фикс. уровень → детерминизм
    out.push_back(std::move(a));
}

// Tools-free бейк (writer+zstd) для CI на всех POSIX: те же 5 guid, что реальный бейк →
// asset_test/determinism/byte-golden гоняются без tint/basisu (cross-machine детерм. writer+zstd).
void synthetic(std::vector<AssetInput>& out) {
    out.push_back(synth_asset("sprite.vs", AssetType::Shader, Codec::SpirV, Residency::Mmap, 1024, 1));
    out.push_back(synth_asset("sprite.fs", AssetType::Shader, Codec::SpirV, Residency::Mmap, 512, 2));
    out.push_back(synth_asset("hero_albedo", AssetType::Texture, Codec::Raw, Residency::Stream, 4096, 3));
    out.push_back(synth_asset("hero_normal", AssetType::Texture, Codec::Raw, Residency::Stream, 4096, 4));
    bulk("scene_bulk", out); // тот же 64KB zstd-паттерн, что реальный бейк
}

} // namespace asset::bakers
