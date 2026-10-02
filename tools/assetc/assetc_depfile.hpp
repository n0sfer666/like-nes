#pragma once
#include <string>
#include <vector>

// depfile бейка в синтаксисе Make (спека #24, В4): `add_custom_command(DEPFILE)` отдаёт его Ninja,
// Makefile, Xcode и Visual Studio. Без него сборка знала бы только манифест, и правка PNG или
// тайлсета, на который ссылается `.tmj`, не перепекала бы бандл.
namespace asset::depfile {

// Одна строка `<цель>: <файл> <файл> …`. Пробел и `#` экранируются `\`, `$` удваивается — иначе
// путь с пробелом развалился бы на два несуществующих файла. `\` переводится в `/`, только когда
// он разделитель (`platform::is_sep`): на POSIX это буква имени, и замена дала бы зависимость от
// несуществующего файла, то есть перебейк на каждой сборке.
std::string text(const std::string& target, const std::vector<std::string>& deps,
                 bool backslash_is_sep);

} // namespace asset::depfile
