#include "platform_watch.hpp"

#include <string>
#include <vector>

#include "platform_fs.hpp"

// iOS: FSEvents — API только macOS, в SDK iOS его нет. Нативного бэкенда наблюдения у этой ОС нет
// вовсе, поэтому шов честно отдаёт опрос: `backend()` говорит Poll, и вызывающий видит, чем именно
// за ним смотрят, а не получает молчаливый отказ. `native_` здесь не создаётся никогда.
namespace platform {

Watcher::~Watcher() { close(); }

void Watcher::close() {
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
    poll_ = detail::poll_open(dir, recursive);
    backend_ = WatchBackend::Poll;
    return poll_ != nullptr;
}

bool Watcher::poll(std::vector<std::string>& changed, int timeout_ms) {
    changed.clear();
    return poll_ != nullptr && detail::poll_step(poll_, changed, timeout_ms);
}

} // namespace platform
