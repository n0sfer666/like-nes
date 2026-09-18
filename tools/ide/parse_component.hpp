#pragma once
#include "scene.hpp"
#include <set>
#include <string>

// Разбор ОДНОЙ строки `C <ComponentName> <flecs-json>` и его диагностика. Отдельно от текстов
// (`deserialize.cpp`) потому, что предмет отказа здесь другой: тексты отвечают за причины 1–8 из
// `serialize.hpp` — порядок строк, шапку, номера сущностей, — а строка за 9–14, и чинятся они в
// разных местах файла. Заголовок ПРИВАТНЫЙ (лежит рядом с .cpp, а не в публичном `serialize.hpp`):
// снаружи у него читателей нет и быть не должно — редактор зовёт `deserialize`/`restore_entity`,
// а не разбор строки (разрез по решению владельца, третий заход ревью A·2·8).
namespace ide::detail {

// Цитата строки в диагностике — обрезанная, с резом по границе символа UTF-8.
std::string quoted(const std::string& s);

// `<where><no>: <msg>` — номер строки идёт ПЕРВЫМ и считается по тексту, который РАЗБИРАЮТ.
std::string at(const char* where, size_t no, const std::string& msg);

// `seen` — имена компонентов, уже прочитанные У ЭТОЙ сущности: дубль имени отказ, причина 11.
// Владеет набором вызывающий: у файла сцены он чистится на каждой `E`, у тела снимка живёт один.
bool apply_line(flecs::world& w, flecs::entity e, uint64_t guid, const std::string& line,
                const char* where, size_t no, std::set<std::string>& seen, std::string* why);

} // namespace ide::detail
