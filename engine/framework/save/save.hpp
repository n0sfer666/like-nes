#pragma once
#include <cstdint>
#include <span>
#include <string>

namespace save {
// Вызывающий сериализует запись одного пути; каталог должен уже существовать.
bool write_atomic(const std::string& path, std::span<const uint8_t> bytes);
} // namespace save
