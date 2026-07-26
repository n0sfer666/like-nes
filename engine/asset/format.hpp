#pragma once
#include <cstdint>

// Target-native zero-parse раскладка бандла (спека #5 / ADR 0003).
// mmap → reinterpret_cast без парсинга: заголовок и таблица — POD фикс-размера,
// payload'ы адресуются offset'ами от базы бандла. Desktop-first: little-endian.
// Смена полей → bump FMT_VERSION (fail-hard reader → rebake, ассеты производны).

namespace asset {

constexpr uint8_t MAGIC[4] = {'L', 'N', 'A', 'B'}; // like-nes asset bundle
constexpr uint32_t FMT_VERSION = 1;
constexpr uint32_t ENDIAN_LE = 1;
constexpr uint32_t PAYLOAD_ALIGN = 16; // SPIR-V нужен 4, берём 16 (SIMD-запас)

enum class AssetType : uint32_t {
    Raw = 0,     // hot/мелкое — mmap zero-copy
    Texture = 1, // KTX2/UASTC → transcode BC7
    Shader = 2,  // SPIR-V backend-IR (Tint), zero-copy в SPIRV-descriptor
    Bulk = 3,    // zstd-блок → декомпрессия в арену
    Audio = 4,   // vorbis/opus-контейнер → decode-worker (спека #3)
};

enum class Codec : uint32_t {
    Raw = 0,       // без сжатия (mmap zero-copy)
    Zstd = 1,      // zstd-блок (decompress → арена)
    Ktx2Uastc = 2, // KTX2 UASTC (basis transcode → BC7)
    SpirV = 3,     // SPIR-V words (raw, zero-copy)
    Vorbis = 4,    // Ogg Vorbis-контейнер (stb_vorbis decode на worker)
};

// Для AssetType::Audio generic-слоты AssetEntry несут аудио-мету (плоско, без union). В PoC
// заполняется только variant_key=flags (bit0=loop); tex_w/tex_h/tex_format (sample_rate/
// channels/frames) ЗАРЕЗЕРВИРОВАНЫ под bake-time мету — сейчас рантайм выводит их из
// ogg-контейнера (stb_vorbis). Заполнять при bake — точка расширения (нужен decode в assetc).
constexpr uint32_t AUDIO_FLAG_LOOP = 1u;

enum class Residency : uint32_t {
    Mmap = 0,   // резидент, zero-copy (index/hot/small/shader)
    Stream = 1, // async-I/O + decompress вне sim-потока (bulk/texture)
};

// Фикс-размер запись таблицы ассетов (плоский массив → reinterpret_cast).
// guid — стабильная ссылка (переживает rename); content_hash — ключ кеша/дедупа.
struct AssetEntry {
    uint64_t guid;
    uint64_t content_hash;    // FNV-1a64 несжатого payload'а
    uint32_t type;            // AssetType
    uint32_t codec;           // Codec
    uint32_t residency;       // Residency
    uint32_t payload_offset;  // от базы бандла, выровнен на PAYLOAD_ALIGN
    uint32_t payload_size;    // на диске (сжато)
    uint32_t uncompressed_size; // размер после декомпрессии (для арены)
    // type-специфичные поля (плоско, без union — zero-parse):
    uint32_t tex_w;
    uint32_t tex_h;
    uint32_t tex_format;   // WGPUTextureFormat (target-native значение)
    uint32_t variant_key;  // shader: ключ комбинации defines (on-demand manifest)
};
static_assert(sizeof(AssetEntry) == 56, "AssetEntry layout pinned (zero-parse ABI)");

struct BundleHeader {
    uint8_t magic[4];
    uint32_t fmt_version;
    uint32_t endian;
    uint32_t header_size;    // == sizeof(BundleHeader)
    uint32_t asset_count;
    uint32_t table_offset;   // от базы бандла
    uint32_t total_size;     // весь бандл в байтах
    uint32_t _pad;           // выравнивание таблицы
    uint64_t bundle_hash;    // FNV-1a64 всех байт бандла с обнулённым этим полем
};
static_assert(sizeof(BundleHeader) == 40, "BundleHeader layout pinned (zero-parse ABI)");

} // namespace asset
