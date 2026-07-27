#pragma once
#include <string>

// Конвертация UTF-8 ↔ UTF-16 для win32-реализаций платформенных швов. Собирается ТОЛЬКО на
// Windows (CMake не добавляет этот TU на POSIX), поэтому условной компиляции внутри нет.
//
// Граница простая: наружу движок говорит только в UTF-8, wide-API — внутренность win32-путей.
// Без этого ANSI-API молча ломает типовой профиль пользователя (`C:\Users\Пётр\`) — путь
// не открывается, а причина выглядит как «файла нет».
namespace platform::win32 {

// Пустой результат = ошибка конвертации ИЛИ пустой вход; пустой путь и так невалиден.
std::wstring widen(const std::string& utf8);
std::string narrow(const wchar_t* utf16, size_t len);

} // namespace platform::win32
