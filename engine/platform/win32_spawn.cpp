#include "win32_spawn.hpp"

#include <cstdlib>

#include "win32_cmdline.hpp"
#include "win32_utf.hpp"

namespace platform::win32 {
namespace {

// Список наследуемых хендлов живёт в сырой памяти переменного размера — RAII вокруг него, чтобы
// ни один из четырёх путей отказа ниже не утёк списком и не забыл DeleteProcThreadAttributeList.
class AttributeList {
public:
    ~AttributeList() {
        if (list_ != nullptr) {
            DeleteProcThreadAttributeList(list_);
            std::free(list_);
        }
    }
    AttributeList(const AttributeList&) = delete;
    AttributeList& operator=(const AttributeList&) = delete;
    AttributeList() = default;

    // handles должен пережить CreateProcessW: список хранит указатель, а не копию.
    bool init(HANDLE* handles, DWORD count) {
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);   // всегда FALSE, отдаёт размер
        list_ = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(std::malloc(bytes));
        if (list_ == nullptr) return false;
        if (InitializeProcThreadAttributeList(list_, 1, 0, &bytes) == 0) {
            std::free(list_);
            list_ = nullptr;
            return false;
        }
        return UpdateProcThreadAttribute(list_, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                         count * sizeof(HANDLE), nullptr, nullptr) != 0;
    }

    LPPROC_THREAD_ATTRIBUTE_LIST get() const { return list_; }

private:
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};

} // namespace

bool spawn_process(const std::vector<std::string>& argv, PROCESS_INFORMATION& pi, HANDLE out) {
    std::vector<std::wstring> wargv;
    wargv.reserve(argv.size());
    for (const std::string& a : argv) {
        std::wstring wide = widen(a);
        // Пустой аргумент легален (уедет как ""), пустой результат конвертации — нет.
        if (wide.empty() && !a.empty()) return false;
        wargv.push_back(std::move(wide));
    }
    std::wstring cmdline = command_line(wargv);

    // NUL вместо /dev/null; хендл наследуемый, иначе ребёнок его не увидит.
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE null_out = INVALID_HANDLE_VALUE;
    if (out == INVALID_HANDLE_VALUE) {
        null_out = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (null_out == INVALID_HANDLE_VALUE) return false;
        out = null_out;
    }

    // STARTF_USESTDHANDLES требует все три хендла валидными и наследуемыми — иначе CreateProcessW
    // отказывает целиком. Собственный stdin ни того, ни другого не гарантирует: у процесса без
    // консоли (служба, GUI-хост редактора) GetStdHandle отдаёт NULL, а унаследованный хендл
    // бывает ненаследуемым. Подставляем NUL — ни один здешний ребёнок stdin не читает.
    DWORD in_flags = 0;
    HANDLE std_in = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE null_in = INVALID_HANDLE_VALUE;
    if (std_in == nullptr || std_in == INVALID_HANDLE_VALUE ||
        GetHandleInformation(std_in, &in_flags) == 0 || (in_flags & HANDLE_FLAG_INHERIT) == 0) {
        null_in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (null_in == INVALID_HANDLE_VALUE) {
            if (null_out != INVALID_HANDLE_VALUE) CloseHandle(null_out);
            return false;
        }
        std_in = null_in;
    }

    // Дубликатов в списке быть не должно: stdout и stderr — один и тот же хендл.
    HANDLE inherited[2] = {std_in, out};
    AttributeList attrs;
    const bool have_attrs = attrs.init(inherited, 2);

    STARTUPINFOEXW six{};
    six.StartupInfo.cb = sizeof(six);
    six.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    six.StartupInfo.hStdInput = std_in;
    six.StartupInfo.hStdOutput = out;
    six.StartupInfo.hStdError = out;
    six.lpAttributeList = attrs.get();

    // lpApplicationName = nullptr: имя программы берётся из строки, а значит работает поиск по
    // PATH и автодобавление .exe — ровно как execvp у POSIX-близнеца.
    // inherit=TRUE обязателен для трёх хендлов выше; EXTENDED_STARTUPINFO_PRESENT сужает
    // наследование до списка. Не собрался список — запускаться без него нельзя: ребёнок утащил бы
    // чужой конец канала, а это тихое зависание вместо честного отказа.
    const BOOL ok = have_attrs &&
                    CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, /*inherit=*/TRUE,
                                   EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                   &six.StartupInfo, &pi);
    if (null_in != INVALID_HANDLE_VALUE) CloseHandle(null_in);
    if (null_out != INVALID_HANDLE_VALUE) CloseHandle(null_out);
    return ok != 0;
}

} // namespace platform::win32
