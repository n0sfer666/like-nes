#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "assetc_kind.hpp"

// Манифест бейка игры (спека #24, В4–В5): `|`-грамматика, одна запись на строку, `#` — комментарий.
//
//   texture|<имя>|pixel|<путь>     PNG → RGBA8 Raw, без внешних инструментов
//   texture|<имя>|hd|<путь>        PNG → KTX2 UASTC через basisu
//   level|<имя>|tiled|<путь>.tmj   карта Tiled → записи секций tilemap, visual и objects
//   level|<имя>|tiled|<путь>.tmj|viewport   то же плюс покрытие слоёв под политикой вьюпорта
//   clips|<имя>|aseprite|<путь>.json        лист Aseprite → строки таблицы clips
//   clips|<имя>|sheet|<путь>.sheet          ровная сетка кадров → строки таблицы clips
//   font|<имя>|bitmask|<путь>.json          маски глифов → текстура-атлас <имя> и строка таблицы fonts
//   credits|<имя>|<путь>.txt                титры чужих ассетов → секция <имя>
//   <вид>|<имя>|<путь><расширение>          вид игры (Kind) против clips манифеста → секция <имя>
//
// Имя — ключ guid текстуры или записи в секциях, пути — относительно каталога манифеста. Чужой вид
// записи — отказ, а не пропуск строки: пропущенная запись всплыла бы у игры дырой в бандле.
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

bool parse(const std::string& text, std::vector<Record>& out, Error& err, std::span<const Kind> kinds = {});
const Kind* find_kind(std::span<const Kind> kinds, const std::string& name);
// Таблица видов игры: имя по лексике записи, расширение `.<суффикс>`, без затенения встроенного и
// без дубля. Отказ — `kind table: …`: ошибка инструмента, а не манифеста.
bool kinds_ok(std::span<const Kind> kinds, std::string& error);

// Лексика записи: имя — `[A-Za-z0-9_.-]`, путь — относительный, через `/`, без `..`, `.`, `:` и `\`.
bool name_ok(const std::string& name);
bool path_ok(const std::string& path, std::string& error);

// Путь записи → файл на диске с ТОЧНЫМ регистром каждого сегмента. macOS и Windows открыли бы
// `Tiles.png` по записи `tiles.png`, и манифест ломался бы только на Linux, у игрока.
bool resolve(const std::string& base_dir, const std::string& rel, std::string& full,
             std::string& error);

} // namespace asset::manifest
