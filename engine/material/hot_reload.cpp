#include "hot_reload.hpp"

#include <cstdio>
#include <vector>

#include "platform_fs.hpp"

namespace mat {
namespace {

std::string base_name(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string dir_name(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

} // namespace

bool HotReload::start(const std::string& wgsl_path) {
    path_ = wgsl_path;
    name_ = base_name(wgsl_path);
    if (!watch_.watch_dir(dir_name(wgsl_path), /*recursive=*/false)) {
        error_ = watch_.error();
        return false;
    }
    return true;
}

ReloadEvent HotReload::poll(Cache& cache, int timeout_ms) {
    std::vector<std::string> changed;
    if (!watch_.poll(changed, timeout_ms)) {
        error_ = watch_.error();
        return ReloadEvent::None;
    }
    bool touched = false;
    for (const std::string& p : changed) touched = touched || base_name(p) == name_;
    if (!touched) return ReloadEvent::None;

    std::string wgsl;
    // Чтение, вернувшее пустоту, — это не пустой шейдер, а середина чужой записи: редактор
    // усекает файл и пишет заново, и между двумя этими шагами он существует нулевой длины.
    // Считать такое правкой значило бы отвергать библиотеку на каждом сохранении.
    if (!platform::read_text(path_, wgsl) || wgsl.empty()) return ReloadEvent::None;

    if (cache.reload(wgsl.c_str(), path_, diag_)) {
        ++reloads_;
        std::printf("[material] hot-reload: %s -> %u pipeline(s) total\n", path_.c_str(),
                    cache.pipelines_created());
        return ReloadEvent::Reloaded;
    }
    ++rejects_;
    // Диагностика ПЕРВОЙ строкой и в том же формате, что у бейка: её разбирает панель редактора
    // (#7), и путь к ней у обоих один. Вторая строка — про то, что сцена жива: без неё «отказ»
    // читается как «всё погасло», а погасить его тут нечему.
    std::fprintf(stderr, "%s\n", format_diag(diag_).c_str());
    std::fprintf(stderr, "[material] hot-reload: rejected, previous library still drawing\n");
    return ReloadEvent::Rejected;
}

} // namespace mat
