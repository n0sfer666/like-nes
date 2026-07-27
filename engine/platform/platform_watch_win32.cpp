#include "platform_watch.hpp"

#include <windows.h>

#include <set>
#include <string>
#include <vector>

#include "platform_fs.hpp"
#include "win32_utf.hpp"

// Нативный бэкенд наблюдения на Windows: ReadDirectoryChangesW. Рекурсию он, в отличие от
// inotify, умеет сам (bWatchSubtree), поэтому дорегистрации подкаталогов здесь нет.
//
// Чтение асинхронное с событием: синхронный вызов блокируется до первого изменения БЕЗ таймаута,
// и poll(timeout_ms) на нём выражался бы только вторым потоком.
namespace platform {
namespace {

constexpr DWORD kFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                          FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;

} // namespace

struct Watcher::Native {
    HANDLE dir = INVALID_HANDLE_VALUE;
    HANDLE event = nullptr;
    OVERLAPPED ov{};
    std::string root;
    bool recursive = false;
    // DWORD-выравнивание требует сама ФС: невыровненный буфер ReadDirectoryChangesW отвергает.
    alignas(DWORD) char buf[16 * 1024] = {};

    bool issue() {
        ResetEvent(event);
        ov = OVERLAPPED{};
        ov.hEvent = event;
        return ReadDirectoryChangesW(dir, buf, sizeof(buf), recursive ? TRUE : FALSE, kFilter,
                                     nullptr, &ov, nullptr) != 0;
    }
};

Watcher::~Watcher() { close(); }

void Watcher::close() {
    if (native_) {
        if (native_->dir != INVALID_HANDLE_VALUE) {
            // Отмена ДО закрытия события: незавершённое чтение иначе допишет в уже освобождённую
            // структуру OVERLAPPED.
            CancelIo(native_->dir);
            CloseHandle(native_->dir);
        }
        if (native_->event) CloseHandle(native_->event);
        delete native_;
        native_ = nullptr;
    }
    detail::poll_close(poll_);
    poll_ = nullptr;
}

bool Watcher::watch_dir(const std::string& dir, bool recursive) {
    if (valid()) {
        error_ = "watcher already started";
        return false;
    }
    if (!is_dir(dir)) {
        error_ = "not a directory: " + dir;
        return false;
    }

    if (!detail::poll_forced()) {
        auto* n = new Native();
        n->root = dir;
        n->recursive = recursive;
        const std::wstring wide = win32::widen(dir);
        if (!wide.empty()) {
            // FILE_FLAG_BACKUP_SEMANTICS — единственный способ получить хендл КАТАЛОГА;
            // делимся всем, иначе наблюдение запрещало бы сборке трогать своё же дерево.
            n->dir = CreateFileW(wide.c_str(), FILE_LIST_DIRECTORY,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                 OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                 nullptr);
            n->event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }
        if (n->dir != INVALID_HANDLE_VALUE && n->event && n->issue()) {
            native_ = n;
            backend_ = WatchBackend::Native;
            return true;
        }
        // Сетевые и виртуализованные ФС отказывают именно здесь — деградируем в поллинг,
        // причину запоминаем: гейт 3 прогоняется в том числе на shared folder виртуалки.
        error_ = "ReadDirectoryChangesW unavailable (error " + std::to_string(GetLastError()) +
                 "), polling";
        if (n->dir != INVALID_HANDLE_VALUE) CloseHandle(n->dir);
        if (n->event) CloseHandle(n->event);
        delete n;
    }

    poll_ = detail::poll_open(dir, recursive);
    backend_ = WatchBackend::Poll;
    return poll_ != nullptr;
}

bool Watcher::poll(std::vector<std::string>& changed, int timeout_ms) {
    changed.clear();
    if (poll_) return detail::poll_step(poll_, changed, timeout_ms);
    if (!native_) return false;

    const DWORD waited = WaitForSingleObject(native_->event, static_cast<DWORD>(timeout_ms));
    if (waited == WAIT_TIMEOUT) return true;
    if (waited != WAIT_OBJECT_0) return false;

    DWORD bytes = 0;
    if (GetOverlappedResult(native_->dir, &native_->ov, &bytes, FALSE) == 0) return native_->issue();

    // Дедупликация: «сохранить» из редактора даёт LAST_WRITE и SIZE на один файл, а rename через
    // временный файл — ещё и пару RENAMED_OLD/NEW. Пересобирать его столько же раз незачем.
    std::set<std::string> unique;
    // bytes == 0 — переполнение буфера: ОС отдала «событий было больше, чем влезло», и ни одного
    // имени. Молчать нельзя, поэтому корень объявляется изменившимся целиком — потребитель
    // пересканирует сам, а не потеряет правку.
    if (bytes == 0) {
        unique.insert(native_->root);
    } else {
        for (DWORD off = 0;;) {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(native_->buf + off);
            const std::string name =
                win32::narrow(info->FileName, info->FileNameLength / sizeof(WCHAR));
            if (!name.empty()) {
                const std::string path = native_->root + "\\" + name;
                // Каталоги наружу не идут — потребителю нужны файлы; отличить их можно только у
                // ещё существующих, у удалённых атрибутов уже нет, и такие сообщаются как есть.
                if (!is_dir(path)) unique.insert(path);
            }
            if (info->NextEntryOffset == 0) break;
            off += info->NextEntryOffset;
        }
    }
    changed.assign(unique.begin(), unique.end());
    return native_->issue();
}

} // namespace platform
