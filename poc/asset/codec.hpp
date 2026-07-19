#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Кодек-по-классу ассета (спека #5): zstd для bulk, Tint для шейдеров (WGSL→SPIR-V),
// basisu для текстур (PNG→KTX2 UASTC). Внешние кодеки вызываются как пиннутые бинарники
// (детерминизм → байт-golden гейт #1). zstd линкуется в процесс.
// Trust-модель: assetc — build-time TRUSTED тул (как компилятор в cmake); пути к tint/basisu
// задаются вызывающим (CI/IDE передаёт абсолютные через --tint/--basisu). PATH-резолв дефолтов
// "tint"/"basisu" — как у любого build-инструмента; недоверенный вход бейку не подаётся.
namespace asset::codec {

struct Tools {
    std::string tint;   // путь к tint CLI
    std::string basisu; // путь к basisu CLI
};

std::vector<uint8_t> read_file(const std::string& path);
bool write_file(const std::string& path, const std::vector<uint8_t>& data);

// zstd-блок (bulk → decompress в арену). Детерминирован при фикс. уровне.
std::vector<uint8_t> zstd_compress(const std::vector<uint8_t>& in, int level);

// WGSL → SPIR-V одного entry point (Tint). Детерминирован, spirv-val-валиден.
bool wgsl_to_spirv(const Tools& t, const std::string& wgsl_path, const std::string& ep,
                   std::vector<uint8_t>& out);

// PNG → KTX2 UASTC (basisu, single-thread = детерминирован). w/h из PNG.
bool png_to_ktx2(const Tools& t, const std::string& png_path, const std::string& tmp_out,
                 std::vector<uint8_t>& out, uint32_t& w, uint32_t& h);

} // namespace asset::codec
