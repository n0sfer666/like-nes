#include "win32_utf.hpp"

#include <windows.h>

#include <climits>

namespace platform::win32 {

// MB_ERR_INVALID_CHARS: битую UTF-8-последовательность отвергаем, а не подменяем U+FFFD —
// иначе путь «почти открылся» и ошибка всплыла бы как отсутствующий файл.
std::wstring widen(const std::string& utf8) {
    if (utf8.empty() || utf8.size() > static_cast<size_t>(INT_MAX)) return {};
    const int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                        static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                            static_cast<int>(utf8.size()), out.data(), len) != len)
        return {};
    return out;
}

std::string narrow(const wchar_t* utf16, size_t len) {
    if (utf16 == nullptr || len == 0 || len > static_cast<size_t>(INT_MAX)) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, utf16, static_cast<int>(len), nullptr, 0,
                                      nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, utf16, static_cast<int>(len), out.data(), n, nullptr,
                            nullptr) != n)
        return {};
    return out;
}

} // namespace platform::win32
