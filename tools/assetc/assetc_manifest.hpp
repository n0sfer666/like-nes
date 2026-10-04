#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Манифест бейка игры (спека #24, В4–В5): `|`-грамматика, одна запись на строку, `#` — комментарий.
//
//   texture|<имя>|pixel|<путь>     PNG → RGBA8 Raw, без внешних инструментов
//   texture|<имя>|hd|<путь>        PNG → KTX2 UASTC через basisu
//   level|<имя>|tiled|<путь>.tmj   карта Tiled → записи секций tilemap, visual и objects
//   level|<имя>|tiled|<путь>.tmj|viewport   то же плюс покрытие слоёв под политикой вьюпорта
//
// Имя — ключ guid текстуры или записи уровня в секциях, пути — относительно каталога манифеста.
// `clips` и `credits` придут со своими пекарями (В6, В8); до тех пор это отказ, а не пропуск
// строки: пропущенная запись всплыла бы у игры дырой в бандле, а не у автора манифеста.
namespace asset::manifest {

struct Record {
    uint32_t line = 0;
    std::string kind, name, codec, path;
    bool viewport = false;
};

struct Error {
    uint32_t line = 0;
    std::string message;
};

bool parse(const std::string& text, std::vector<Record>& out, Error& err);

// Путь записи → файл на диске с ТОЧНЫМ регистром каждого сегмента. macOS и Windows открыли бы
// `Tiles.png` по записи `tiles.png`, и манифест ломался бы только на Linux, у игрока.
bool resolve(const std::string& base_dir, const std::string& rel, std::string& full,
             std::string& error);

} // namespace asset::manifest
