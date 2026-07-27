#include "platform_watch.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "platform_env.hpp"
#include "platform_fs.hpp"
#include "platform_path.hpp"

// Фолбэк-поллинг шва наблюдения. Собран целиком из platform_fs, поэтому один на три ОС и стоит
// вне ветки CMake: дублировать обход дерева в каждой платформенной TU значило бы чинить его
// трижды.
namespace platform {
namespace {

std::string join(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    return is_sep(dir.back()) ? dir + name : dir + "/" + name;
}

// std::map, а не unordered: сравнение двух снимков идёт линейным слиянием по отсортированным
// ключам, и порядок путей в changed получается стабильным — иначе один и тот же набор правок
// приезжал бы в разном порядке от прогона к прогону, и гейт на нём было бы не написать.
using Snapshot = std::map<std::string, int64_t>;

void scan(const std::string& dir, bool recursive, Snapshot& out) {
    std::vector<std::string> names;
    if (!list_dir(dir, names)) return;
    for (const std::string& name : names) {
        const std::string path = join(dir, name);
        if (is_dir(path)) {
            if (recursive) scan(path, true, out);
            continue;
        }
        int64_t stamp = 0;
        if (file_stamp(path, stamp)) out.emplace(path, stamp);
    }
}

// Слияние двух отсортированных снимков: появившиеся, исчезнувшие и сменившие штамп — всё это
// изменения. Удаление сообщается наравне с правкой: исчезнувший исходник обязан вынести из
// сборки свой объектник, а не остаться в ней последней удачной версией.
void diff(const Snapshot& before, const Snapshot& after, std::vector<std::string>& changed) {
    auto a = before.begin();
    auto b = after.begin();
    while (a != before.end() || b != after.end()) {
        if (b == after.end() || (a != before.end() && a->first < b->first)) {
            changed.push_back(a->first);
            ++a;
        } else if (a == before.end() || b->first < a->first) {
            changed.push_back(b->first);
            ++b;
        } else {
            if (a->second != b->second) changed.push_back(a->first);
            ++a;
            ++b;
        }
    }
}

// Шаг сна между пересканами. Меньше — заметнее нагрузка на большом дереве, больше — заметнее
// задержка build-loop на глаз; 50 мс сидит ниже порога восприятия и не жжёт диск.
constexpr int kSliceMs = 50;

} // namespace

struct Watcher::Polling {
    std::string dir;
    bool recursive = false;
    Snapshot snapshot;
};

namespace detail {

bool poll_forced() {
    std::string mode;
    return env_var("LIKE_NES_WATCH", mode) && mode == "poll";
}

Watcher::Polling* poll_open(const std::string& dir, bool recursive) {
    if (!is_dir(dir)) return nullptr;
    auto* p = new Watcher::Polling();
    p->dir = dir;
    p->recursive = recursive;
    // Базовый снимок снимается СРАЗУ: иначе первый же poll объявил бы изменившимся всё дерево.
    scan(dir, recursive, p->snapshot);
    return p;
}

void poll_close(Watcher::Polling* p) { delete p; }

bool poll_step(Watcher::Polling* p, std::vector<std::string>& changed, int timeout_ms) {
    if (!p) return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        Snapshot fresh;
        scan(p->dir, p->recursive, fresh);
        diff(p->snapshot, fresh, changed);
        if (!changed.empty()) {
            p->snapshot.swap(fresh);
            return true;
        }
        // Сравнение с дедлайном — ПОСЛЕ первого скана: timeout_ms=0 обязан остаться честным
        // опросом без ожидания, а не «не смотреть вовсе».
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return true;
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        std::this_thread::sleep_for(std::min(left, std::chrono::milliseconds(kSliceMs)));
    }
}

} // namespace detail
} // namespace platform
