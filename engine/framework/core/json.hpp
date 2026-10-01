#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "byte_arena.hpp"
#include "fixed.hpp"

// Разборщик JSON для пекарей контента (спека #24, В3): Tiled `.tmj`/`.tsj` и листы Aseprite.
// Рантайм JSON не разбирает — всё отсюда печётся в zero-parse секции, поэтому цели нет в SDK.
//
// Дерево плоское: корень — `out[0]`, дети контейнера лежат подряд с `first`. У массива `count`
// элементов, у объекта `count` ПАР «ключ, значение», то есть `2 * count` узлов. Строки без
// экранирования — срезы входа, с экранированием — срезы арены: и вход, и арена обязаны пережить
// дерево.
namespace framework::json {

enum class Kind : uint8_t { Null, Bool, Int, Fix, String, Array, Object };

struct Node {
    Kind kind = Kind::Null;
    uint32_t first = 0, count = 0;
    // Int и Bool (0/1). Целое без точки и порядка — `int64_t`, а не fix32: id и gid Tiled не
    // помещаются в Q16.16, а тихое приведение потеряло бы их старшие разряды.
    int64_t i = 0;
    fix32 f;
    std::string_view s;
    // Смещение начала значения во входе — для ошибок пекаря уровнем выше («тип слоя не тот»),
    // которые обязаны называть строку и столбец так же, как ошибки самого разбора.
    uint32_t at = 0;
};

struct JsonError {
    uint32_t line = 0, column = 0;
    std::string message;
};

constexpr uint32_t MAX_DEPTH = 64;
constexpr std::size_t MAX_DOCUMENT = std::size_t{64} << 20;
constexpr std::size_t MAX_STRING = std::size_t{1} << 20;
constexpr std::size_t MAX_NODES = std::size_t{1} << 21;

// Превышение лимита — отказ, а не обрезка. Арена нужна под строки с экранированием: ёмкости размера
// входа хватает всегда, декодированная строка не длиннее своей записи.
bool parse(std::span<const std::byte> in, asset::ByteArena& arena, std::vector<Node>& out,
           JsonError& err);

const Node* member(std::span<const Node> doc, const Node& obj, std::string_view key);

// Строка и столбец (с единицы, столбец — в символах UTF-8) смещения `at` во входе.
JsonError error_at(std::span<const std::byte> in, uint32_t at, std::string message);

// `level1.tmj:12:7: expected ','`
std::string format_error(std::string_view file, const JsonError& err);

} // namespace framework::json
