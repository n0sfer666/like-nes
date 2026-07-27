#include "platform_watch.hpp"

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

#include <climits>
#include <cstdlib>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "platform_fs.hpp"

// Нативный бэкенд наблюдения на macOS: FSEvents. В отличие от inotify он приходит НЕ по запросу,
// а колбэком, поэтому события складываются в очередь под мьютексом, а poll() её разбирает. Тем
// самым наружу шов остаётся опросом на всех трёх ОС (решение 4 спеки #13), а не двумя разными
// формами API.
//
// Колбэк доставляет dispatch-очередь, а не собственный CFRunLoop: run-loop-вариант API объявлен
// устаревшим в macOS 13, а свой поток с CFRunLoopRun ради этого пришлось бы ещё и уметь
// останавливать снаружи.
namespace platform {
namespace {

// FSEvents всегда рекурсивен от корня наблюдения, отключить это нечем. Для recursive=false
// глубина фильтруется здесь: иначе флаг молча ничего не значил бы на одной ОС из трёх.
bool direct_child(const std::string& dir, const std::string& path) {
    if (path.size() <= dir.size() + 1 || path.compare(0, dir.size(), dir) != 0) return false;
    return path.find('/', dir.size() + 1) == std::string::npos;
}

// Задержка агрегации событий. Ниже неё FSEvents будит процесс на каждую запись, выше —
// build-loop ощутимо «думает» перед пересборкой.
constexpr CFAbsoluteTime kLatencySec = 0.05;

} // namespace

struct Watcher::Native {
    std::string root;
    bool recursive = false;
    std::mutex mtx;
    std::condition_variable cv;
    std::set<std::string> pending;
    FSEventStreamRef stream = nullptr;
    dispatch_queue_t queue = nullptr;

    void push(const std::string& path) {
        if (is_dir(path)) return;   // потребителю нужны файлы; каталог сам по себе не пересобрать
        if (!recursive && !direct_child(root, path)) return;
        {
            std::lock_guard<std::mutex> lock(mtx);
            pending.insert(path);
        }
        cv.notify_all();
    }
};

namespace {

void fsevents_cb(ConstFSEventStreamRef, void* ctx, size_t count, void* paths,
                 const FSEventStreamEventFlags*, const FSEventStreamEventId*) {
    auto* n = static_cast<Watcher::Native*>(ctx);
    auto** items = static_cast<char**>(paths);
    for (size_t i = 0; i < count; ++i) n->push(items[i]);
}

bool start_stream(Watcher::Native* n) {
    CFStringRef path = CFStringCreateWithCString(nullptr, n->root.c_str(), kCFStringEncodingUTF8);
    if (!path) return false;
    CFArrayRef paths =
        CFArrayCreate(nullptr, reinterpret_cast<const void**>(&path), 1, &kCFTypeArrayCallBacks);
    FSEventStreamContext ctx{0, n, nullptr, nullptr, nullptr};
    // kFSEventStreamCreateFlagFileEvents: без него колбэк отдаёт КАТАЛОГ, а не файл, и build-loop
    // не знал бы, что именно пересобирать.
    n->stream = FSEventStreamCreate(nullptr, &fsevents_cb, &ctx, paths, kFSEventStreamEventIdSinceNow,
                                    kLatencySec, kFSEventStreamCreateFlagFileEvents |
                                                     kFSEventStreamCreateFlagNoDefer);
    CFRelease(paths);
    CFRelease(path);
    if (!n->stream) return false;

    n->queue = dispatch_queue_create("likenes.watch", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(n->stream, n->queue);
    if (FSEventStreamStart(n->stream) == 0) {
        FSEventStreamInvalidate(n->stream);
        FSEventStreamRelease(n->stream);
        n->stream = nullptr;
        return false;
    }
    return true;
}

} // namespace

Watcher::~Watcher() { close(); }

void Watcher::close() {
    if (native_) {
        if (native_->stream) {
            FSEventStreamStop(native_->stream);
            // Invalidate — граница, после которой колбэк не вызовут; без неё очередь могла бы
            // дописать в уже удалённый объект.
            FSEventStreamInvalidate(native_->stream);
            FSEventStreamRelease(native_->stream);
        }
        if (native_->queue) dispatch_release(native_->queue);
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
        // Корень канонизируется: FSEvents отдаёт разрешённый путь (/tmp → /private/tmp), и
        // фильтр direct_child сравнивал бы его с исходной формой, не совпадая никогда.
        char real[PATH_MAX];
        n->root = realpath(dir.c_str(), real) ? std::string(real) : dir;
        n->recursive = recursive;
        if (start_stream(n)) {
            native_ = n;
            backend_ = WatchBackend::Native;
            return true;
        }
        error_ = "FSEvents unavailable, polling";
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

    std::unique_lock<std::mutex> lock(native_->mtx);
    native_->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                         [&] { return !native_->pending.empty(); });
    changed.assign(native_->pending.begin(), native_->pending.end());
    native_->pending.clear();
    return true;
}

} // namespace platform
