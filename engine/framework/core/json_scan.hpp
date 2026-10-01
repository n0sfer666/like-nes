#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "byte_arena.hpp"
#include "json.hpp"

// Лексика разборщика, общая для его единиц трансляции. Не публичный заголовок: строку и число
// вызывающий получает только узлом дерева.
namespace framework::json::detail {

struct Cursor {
    const char* data = nullptr;
    std::size_t size = 0, pos = 0;
    asset::ByteArena* arena = nullptr;
    std::size_t fail_at = 0;
    std::string fail;

    int peek() const { return pos < size ? static_cast<unsigned char>(data[pos]) : -1; }
    int byte_at(std::size_t i) const { return i < size ? static_cast<unsigned char>(data[i]) : -1; }
    bool error(std::size_t at, std::string message) {
        fail_at = at;
        fail = std::move(message);
        return false;
    }
};

// Курсор стоит на открывающей кавычке и уходит за закрывающую.
bool scan_string(Cursor& c, std::string_view& out);
// Курсор стоит на первом символе числа (`-` или цифра).
bool scan_number(Cursor& c, Node& out);
// Первые 64 байта ключа для текста ошибки: байт вне печатного ASCII, кавычка и слэш — `\xNN`.
std::string printable(std::string_view key);

} // namespace framework::json::detail
