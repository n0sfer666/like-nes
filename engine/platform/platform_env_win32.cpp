#include "platform_env.hpp"

#include <windows.h>

#include <utility>
#include <vector>

#include "win32_utf.hpp"

namespace platform {

bool env_var(const char* name, std::string& out) {
    const std::wstring wname = win32::widen(name);
    if (wname.empty()) return false;
    std::vector<wchar_t> buf(256);
    SetLastError(ERROR_SUCCESS);
    DWORD n = GetEnvironmentVariableW(wname.c_str(), buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0) {
        // Ноль — это и «переменной нет», и «значение пустое»; различает только код ошибки, и
        // только если его сбросить самому: на успехе функция last-error не трогает. Ветка ради
        // симметрии с POSIX, где getenv на пустую отдаёт "" и это true; сама Windows в это
        // состояние попадает разве что из блока окружения CreateProcessW.
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) return false;
        out.clear();
        return true;
    }
    if (n >= buf.size()) { // n здесь — требуемый размер С нулём, а не длина значения
        buf.resize(n);
        n = GetEnvironmentVariableW(wname.c_str(), buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0 || n >= buf.size()) return false;
    }
    std::string narrowed = win32::narrow(buf.data(), n);
    // narrow отдаёт пустую строку и на отказ конвертации тоже. Отдать её как значение — то же
    // «ассет не нашёлся» вместо честного «переменная не читается»: непустое UTF-16 в пустой UTF-8
    // не превращается.
    if (narrowed.empty()) return false;
    out = std::move(narrowed);
    return true;
}

bool env_put(const char* name, const char* value) {
    const std::wstring wname = win32::widen(name);
    if (wname.empty()) return false;
    std::wstring wvalue;
    if (value != nullptr) {
        wvalue = win32::widen(value);
        // widen пуст и на отказ конвертации, и на пустой вход, а пустая строка здесь — команда
        // УДАЛИТЬ. Без этой развилки битый UTF-8 стирал бы переменную и рапортовал успех.
        if (wvalue.empty() && value[0] != '\0') return false;
    }
    // Оба документированных способа удаления — nullptr и пустая строка — ведутся одной развилкой:
    // разойдись они по возвращаемому значению, вызывающий получил бы ту же платформенную развилку,
    // которую шов и обязан прятать.
    const bool removing = value == nullptr || wvalue.empty();
    if (SetEnvironmentVariableW(wname.c_str(), value == nullptr ? nullptr : wvalue.c_str()))
        return true;
    // Удаление отсутствующей переменной здесь — ошибка, а у POSIX-unsetenv успех. Разводить
    // вызывающих по ОС из-за этого нельзя: приводим к поведению unsetenv.
    return removing && GetLastError() == ERROR_ENVVAR_NOT_FOUND;
}

} // namespace platform
