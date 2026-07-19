#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bundle_writer.hpp"
#include "codec.hpp"
#include "format.hpp"
#include "hash.hpp"

// Headless CLI-бейкер (спека #5): source (png+wgsl+bulk) → детерм. bundle.
// Один код бейка (CI + IDE-watch поверх). Печатает golden bundle_hash (гейт #1).

using namespace asset;

namespace {

// GUID = FNV логического имени → стабилен (переживает rename файла), детерминирован cross-machine.
uint64_t guid_of(const char* name) { return fnv1a(name, std::strlen(name)); }

uint32_t bc7_size(uint32_t w, uint32_t h) {
    return ((w + 3) / 4) * ((h + 3) / 4) * 16; // 1 BC7-блок 4x4 = 16 байт
}

bool bake_texture(const codec::Tools& t, const std::string& src, const char* name,
                  const std::string& tmp, std::vector<AssetInput>& out) {
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

bool bake_shader(const codec::Tools& t, const std::string& src, const char* name,
                 const std::string& ep, uint32_t stage, std::vector<AssetInput>& out) {
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

void bake_bulk(const char* name, std::vector<AssetInput>& out); // fwd

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0) { std::fclose(f); return false; }
    out.resize(static_cast<size_t>(n));
    size_t rd = std::fread(out.data(), 1, static_cast<size_t>(n), f);
    std::fclose(f);
    return rd == static_cast<size_t>(n);
}

// Аудио (спека #3): vorbis-контейнер как Mmap zero-copy (декод в рантайме stb_vorbis).
// Бейк детерминирован — копирует committed .ogg байты (энкодер не нужен в CI).
bool bake_audio(const std::string& src, const char* name, bool loop, std::vector<AssetInput>& out) {
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

// Синтетический ассет (raw payload детерм. паттерна) — без внешних кодеков.
AssetInput synth_asset(const char* name, AssetType type, Codec codec, Residency res,
                       uint32_t size, uint8_t seed) {
    std::vector<uint8_t> raw(size);
    for (uint32_t i = 0; i < size; ++i) raw[i] = static_cast<uint8_t>((i + seed) * 131u);
    AssetInput a;
    a.guid = guid_of(name);
    a.type = type; a.codec = codec; a.residency = res;
    a.uncompressed_size = size;
    a.payload = std::move(raw);
    return a;
}

// Tools-free бейк (writer+zstd) для CI на всех POSIX: те же 5 guid, что реальный бейк →
// asset_test/determinism/byte-golden гоняются без tint/basisu (cross-machine детерм. writer+zstd).
void bake_synthetic(std::vector<AssetInput>& out) {
    out.push_back(synth_asset("sprite.vs", AssetType::Shader, Codec::SpirV, Residency::Mmap, 1024, 1));
    out.push_back(synth_asset("sprite.fs", AssetType::Shader, Codec::SpirV, Residency::Mmap, 512, 2));
    out.push_back(synth_asset("hero_albedo", AssetType::Texture, Codec::Raw, Residency::Stream, 4096, 3));
    out.push_back(synth_asset("hero_normal", AssetType::Texture, Codec::Raw, Residency::Stream, 4096, 4));
    bake_bulk("scene_bulk", out); // тот же 64KB zstd-паттерн, что реальный бейк
}

void bake_bulk(const char* name, std::vector<AssetInput>& out) {
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

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: assetc <src-dir> <out.bundle> [--tint P] [--basisu P]\n"
                             "       assetc --synthetic <out.bundle>  (tools-free, для CI)\n");
        return 2;
    }
    // Tools-free синтетический бейк (CI): writer+zstd, без tint/basisu.
    if (std::strcmp(argv[1], "--synthetic") == 0) {
        std::vector<AssetInput> synth;
        bake_synthetic(synth);
        std::vector<uint8_t> bundle = write_bundle(std::move(synth));
        if (!codec::write_file(argv[2], bundle)) return 1;
        const BundleHeader* h = reinterpret_cast<const BundleHeader*>(bundle.data());
        std::printf("[assetc] synthetic %s (%u bytes, %u assets)\n", argv[2], h->total_size,
                    h->asset_count);
        std::printf("[assetc] bundle_hash = 0x%016llx\n", (unsigned long long)h->bundle_hash);
        return 0;
    }
    // Аудио-бандл (спека #3): committed .ogg → Mmap/Vorbis. Отдельный бандл — golden-хеши
    // рендер/ассет-бейков не трогаются.
    if (std::strcmp(argv[1], "--audio") == 0) {
        if (argc < 4) {
            std::fprintf(stderr, "usage: assetc --audio <src-dir> <out.bundle>\n");
            return 2;
        }
        std::string asrc = argv[2];
        std::vector<AssetInput> assets;
        bool ok = bake_audio(asrc + "/sfx_ping.ogg", "sfx_ping", false, assets);
        ok = ok && bake_audio(asrc + "/music.ogg", "music", true, assets);
        if (!ok) return 1;
        std::vector<uint8_t> bundle = write_bundle(std::move(assets));
        if (!codec::write_file(argv[3], bundle)) return 1;
        const BundleHeader* h = reinterpret_cast<const BundleHeader*>(bundle.data());
        std::printf("[assetc] audio %s (%u bytes, %u assets)\n", argv[3], h->total_size,
                    h->asset_count);
        std::printf("[assetc] bundle_hash = 0x%016llx\n", (unsigned long long)h->bundle_hash);
        return 0;
    }

    std::string src = argv[1];
    std::string out_path = argv[2];
    codec::Tools tools{"tint", "basisu"};
    for (int i = 3; i + 1 < argc; i += 2) {
        if (std::strcmp(argv[i], "--tint") == 0) tools.tint = argv[i + 1];
        else if (std::strcmp(argv[i], "--basisu") == 0) tools.basisu = argv[i + 1];
    }

    std::vector<AssetInput> assets;
    const std::string tmp = out_path + ".ktx2.tmp";
    bool ok = true;
    ok = ok && bake_texture(tools, src + "/hero_albedo.png", "hero_albedo", tmp, assets);
    ok = ok && bake_texture(tools, src + "/hero_normal.png", "hero_normal", tmp, assets);
    ok = ok && bake_shader(tools, src + "/sprite.wgsl", "sprite.vs", "vs_main", 0, assets);
    ok = ok && bake_shader(tools, src + "/sprite.wgsl", "sprite.fs", "fs_main", 1, assets);
    if (!ok) return 1;
    bake_bulk("scene_bulk", assets);

    std::vector<uint8_t> bundle = write_bundle(std::move(assets));
    if (!codec::write_file(out_path, bundle)) {
        std::fprintf(stderr, "[assetc] write failed: %s\n", out_path.c_str());
        return 1;
    }

    const BundleHeader* h = reinterpret_cast<const BundleHeader*>(bundle.data());
    std::printf("[assetc] wrote %s (%u bytes, %u assets)\n", out_path.c_str(), h->total_size,
                h->asset_count);
    std::printf("[assetc] bundle_hash = 0x%016llx\n", (unsigned long long)h->bundle_hash);
    return 0;
}
