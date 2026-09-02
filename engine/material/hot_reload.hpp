#pragma once

#include <string>

#include "cache.hpp"
#include "diag.hpp"
#include "platform_watch.hpp"

namespace mat {

// Правка `.wgsl` → живая сцена (гейт 3 спеки #18). Драйвер живёт РЯДОМ с кэшем, а не у игры и не у
// редактора: потребителей у него двое, и написанный дважды он разъедется ровно в том, ради чего
// написан, — в поведении на БИТОЙ правке. Один из двух молча гасил бы сцену, и узнать это можно
// было бы только глазами.
enum class ReloadEvent {
    None,        // за окно ожидания правок не было
    Reloaded,    // библиотека заменена целиком
    Rejected,    // правка не собралась: жив прежний вариант, причина в diag()
};

class HotReload {
public:
    // Наблюдение идёт за КАТАЛОГОМ файла, а не за файлом: типовое «сохранить» редактора — это
    // запись во временный файл и rename поверх, после которого вотч на самом файле смотрит на
    // удалённый inode и молчит навсегда.
    bool start(const std::string& wgsl_path);

    ReloadEvent poll(Cache& cache, int timeout_ms);

    const ShaderDiag& diag() const { return diag_; }
    uint32_t reloads() const { return reloads_; }
    uint32_t rejects() const { return rejects_; }
    bool watching() const { return watch_.valid(); }
    platform::WatchBackend backend() const { return watch_.backend(); }
    const std::string& error() const { return error_; }

private:
    platform::Watcher watch_;
    std::string path_;
    std::string name_;
    std::string error_;
    ShaderDiag diag_;
    uint32_t reloads_ = 0;
    uint32_t rejects_ = 0;
};

} // namespace mat
