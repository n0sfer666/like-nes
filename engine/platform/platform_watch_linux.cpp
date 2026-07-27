#include "platform_watch.hpp"

#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "platform_fs.hpp"
#include "platform_path.hpp"

// Нативный бэкенд наблюдения на Linux: inotify. Рекурсию inotify не умеет вовсе — каждый
// подкаталог это отдельный вотч, и созданный уже после старта каталог приходится
// дорегистрировать по событию, иначе его содержимое немо.
namespace platform {
namespace {

// IN_MODIFY даёт событие на каждую запись, IN_CLOSE_WRITE — одно на закрытие файла; нужны оба:
// редактор, пишущий через временный файл и rename, не закрывает целевой вовсе, и без MOVED_TO
// правка была бы не видна.
constexpr uint32_t kMask = IN_CLOSE_WRITE | IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM |
                           IN_MOVED_TO;

std::string join(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    return is_sep(dir.back()) ? dir + name : dir + "/" + name;
}

} // namespace

struct Watcher::Native {
    int fd = -1;
    bool recursive = false;
    std::map<int, std::string> dirs;   // wd → каталог, чтобы событие получило полный путь

    // `found` не пустой, когда каталог регистрируется УЖЕ ПО СОБЫТИЮ: между mkdir и нашим
    // inotify_add_watch есть окно, и файлы, созданные в нём, не дадут события никогда. Поэтому
    // содержимое только что взятого под наблюдение каталога считается изменившимся — иначе
    // «создал каталог и сразу файл в нём» теряется целиком, а это ровно то, что делает сборка.
    bool add(const std::string& dir, std::set<std::string>* found = nullptr) {
        const int wd = inotify_add_watch(fd, dir.c_str(), kMask);
        if (wd < 0) return false;
        dirs[wd] = dir;
        if (!recursive) return true;
        std::vector<std::string> names;
        if (!list_dir(dir, names)) return true;
        for (const std::string& name : names) {
            const std::string child = join(dir, name);
            // Отказ на ОДНОМ подкаталоге не рушит наблюдение целиком: у дерева исходников
            // встречаются каталоги без прав, и терять из-за них весь build-loop незачем.
            if (is_dir(child))
                add(child, found);
            else if (found)
                found->insert(child);
        }
        return true;
    }
};

Watcher::~Watcher() { close(); }

void Watcher::close() {
    if (native_) {
        if (native_->fd >= 0) ::close(native_->fd);
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
        n->recursive = recursive;
        n->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (n->fd >= 0 && n->add(dir)) {
            native_ = n;
            backend_ = WatchBackend::Native;
            return true;
        }
        // Исчерпанный лимит вотчей и сетевая ФС отказывают именно здесь. Это не отказ шва:
        // деградируем в поллинг и запоминаем причину, чтобы её было видно в прогоне гейта.
        error_ = std::string("inotify unavailable (") + std::strerror(errno) + "), polling";
        if (n->fd >= 0) ::close(n->fd);
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

    pollfd pfd{native_->fd, POLLIN, 0};
    const int ready = ::poll(&pfd, 1, timeout_ms);
    if (ready < 0) return errno == EINTR;   // прерванное ожидание — не поломка наблюдения
    if (ready == 0) return true;

    // Дедупликация путей внутри окна: типовое «сохранить» даёт MODIFY+CLOSE_WRITE на один файл,
    // и без set сборка запускалась бы дважды подряд.
    std::set<std::string> unique;
    alignas(inotify_event) char buf[8192];
    for (;;) {
        const ssize_t n = ::read(native_->fd, buf, sizeof(buf));
        if (n <= 0) break;
        for (ssize_t off = 0; off + static_cast<ssize_t>(sizeof(inotify_event)) <= n;) {
            const auto* e = reinterpret_cast<const inotify_event*>(buf + off);
            off += static_cast<ssize_t>(sizeof(inotify_event) + e->len);
            const auto it = native_->dirs.find(e->wd);
            if (it == native_->dirs.end() || e->len == 0) continue;
            const std::string path = join(it->second, e->name);
            if ((e->mask & IN_ISDIR) != 0) {
                // Новый каталог берётся под наблюдение вместе с тем, что уже успело в нём
                // появиться (гонка mkdir→вотч); сами каталоги в changed не идут — потребителю
                // нужны файлы.
                if (native_->recursive && (e->mask & (IN_CREATE | IN_MOVED_TO)) != 0)
                    native_->add(path, &unique);
                continue;
            }
            unique.insert(path);
        }
    }
    changed.assign(unique.begin(), unique.end());
    return true;
}

} // namespace platform
