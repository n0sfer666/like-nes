#pragma once
#include <string>
#include <vector>

// Сборка командной строки для CreateProcessW. Собирается ТОЛЬКО на Windows.
//
// Отдельная единица, а не приватная функция process-шва: у CreateProcessW нет argv, ребёнок
// разбирает одну строку обратно правилами CommandLineToArgvW, и это единственное место шва,
// где ошибка тихая — путь с пробелом или кавычкой уедет не тем аргументом. Вынесено, чтобы
// проверяться тестом на round-trip через сам CommandLineToArgvW.
namespace platform::win32 {

// argv → строка, которую CommandLineToArgvW разберёт обратно в тот же argv.
std::wstring command_line(const std::vector<std::wstring>& argv);

} // namespace platform::win32
