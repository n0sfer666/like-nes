#pragma once
#include <span>
#include <string>
#include <vector>

#include "assetc_kind.hpp"
#include "bundle_writer.hpp"
#include "codec.hpp"

// Бейк по манифесту (спека #24, В4): записи → ассеты бандла и список прочитанных файлов для
// depfile. Запись бандла и depfile на диск — у вызывающего: пекарь не знает, куда кладётся выход.
namespace asset::manifest {

struct Bake {
    std::string manifest;     // путь к `.manifest`; пути записей — от его каталога
    codec::Tools tools;       // basisu для записей hd
    std::string tmp;          // временный файл basisu
    std::span<const Kind> kinds;    // виды записей игры, пекутся после clips и fonts
    std::vector<AssetInput> assets;
    std::vector<std::string> deps;  // манифест и каждый прочитанный файл, в порядке чтения
    std::string error;        // `<манифест>:<строка>: …` по-английски
};

bool bake(Bake& b);

// Отказ записи манифеста: `b.error = <манифест>:<строка>: <message>`, всегда false.
bool bake_fail(Bake& b, uint32_t line, const std::string& message);

} // namespace asset::manifest
