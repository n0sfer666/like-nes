#pragma once
#include <string>
#include <vector>

#include "bundle_writer.hpp"
#include "codec.hpp"

// Бейк по манифесту (спека #24, В4): записи → ассеты бандла и список прочитанных файлов для
// depfile. Запись бандла и depfile на диск — у вызывающего: пекарь не знает, куда кладётся выход.
namespace asset::manifest {

struct Bake {
    std::string manifest;     // путь к `.manifest`; пути записей — от его каталога
    codec::Tools tools;       // basisu для записей hd
    std::string tmp;          // временный файл basisu
    std::vector<AssetInput> assets;
    std::vector<std::string> deps;  // манифест и каждый прочитанный файл, в порядке чтения
    std::string error;        // `<манифест>:<строка>: …` по-английски
};

bool bake(Bake& b);

} // namespace asset::manifest
