#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bake.hpp"
#include "cache.hpp"
#include "gpu.hpp"
#include "hot_reload.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "table.hpp"

// Гейт 3 спеки #18: правка `.wgsl` доезжает до живого кэша, а БИТАЯ правка не гасит сцену.
//
// Утверждение здесь ровно одно и оно про состояние ПОСЛЕ отказа: те же самые объекты пайплайнов,
// что были до правки. Проверять его в одиночку нельзя — кэш, который не перезагружается вовсе,
// проходит такую проверку идеально. Поэтому первой идёт ВЕРНАЯ правка: она обязана сменить
// объекты, и только после этого «не сменились» становится утверждением о починке, а не о трупе.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

bool write_file(const std::string& path, const std::string& text) {
    std::FILE* f = platform::open_file(path, "wb");
    if (!f) return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    platform::sync_file(f);
    std::fclose(f);
    platform::sync_dir_of(path);
    return ok;
}

// Правка ждётся ОПРОСАМИ, а не одним окном: нативные бэкенды наблюдения доставляют событие с
// собственной задержкой (у FSEvents она около секунды), и единственное окно, промахнувшееся мимо
// неё, читалось бы как «hot-reload не работает».
mat::ReloadEvent wait_event(mat::HotReload& hot, mat::Cache& cache) {
    for (int i = 0; i < 40; ++i) {
        const mat::ReloadEvent e = hot.poll(cache, 500);
        if (e != mat::ReloadEvent::None) return e;
    }
    return mat::ReloadEvent::None;
}

std::vector<WGPURenderPipeline> snapshot(mat::Cache& cache, uint32_t count) {
    std::vector<WGPURenderPipeline> v;
    for (uint32_t i = 0; i < count; ++i) v.push_back(cache.pipeline(i));
    return v;
}

bool all_replaced(const std::vector<WGPURenderPipeline>& a,
                  const std::vector<WGPURenderPipeline>& b) {
    if (a.size() != b.size() || a.empty()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i] == b[i]) return false;
    return true;
}

std::string base_name(const std::string& p) {
    const std::size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    platform::Args utf8_argv(argc, argv);
    std::string dir = "engine/material/library";
    std::string work = "hot_reload_work";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--library") == 0 && i + 1 < argc) dir = argv[++i];
        else if (std::strcmp(argv[i], "--work") == 0 && i + 1 < argc) work = argv[++i];
    }

    // Правится КОПИЯ: гейт, редактирующий библиотеку в дереве, оставляет её битой ровно тогда,
    // когда падает сам, — и следующий прогон обвиняет в этом движок.
    std::string src, wgsl;
    if (!platform::ensure_dir(work) || !platform::read_text(dir + "/library.mat", src) ||
        !platform::read_text(dir + "/sprite_effects.wgsl", wgsl)) {
        std::printf("hot-reload: FAIL (library not readable under %s)\n", dir.c_str());
        return 1;
    }
    const std::string wgsl_path = work + "/sprite_effects.wgsl";
    if (!write_file(wgsl_path, wgsl)) {
        std::printf("hot-reload: FAIL (work copy not writable under %s)\n", work.c_str());
        return 1;
    }

    std::vector<uint8_t> bytes;
    mat::BakeError err;
    mat::Table table;
    if (!mat::bake_materials(src, bytes, err) ||
        table.load(bytes.data(), bytes.size()) != mat::LoadResult::Ok) {
        std::printf("hot-reload: FAIL (library.mat:%d: %s)\n", err.line, err.message.c_str());
        return 1;
    }

    GpuContext gpu;
    if (!gpu.init(nullptr)) {
        std::printf("hot-reload: FAIL (no adapter)\n");
        gpu.shutdown();
        return 1;
    }
    mat::CacheDesc d;
    d.device = gpu.device; d.queue = gpu.queue; d.table = &table; d.wgsl = wgsl.c_str();
    mat::Cache cache;
    if (!cache.init(d)) {
        std::printf("hot-reload: FAIL (cache did not start)\n");
        gpu.shutdown();
        return 1;
    }
    cache.warm_up();
    std::printf("  warm-up: %u pipeline(s) for %u material(s), %u fallback(s)\n",
                cache.pipelines_created(), table.count(), cache.fallbacks());
    check(cache.pipelines_created() == 3, "the library warms up into three pipelines");
    check(cache.fallbacks() == 0, "nothing in the library falls back");
    const std::vector<WGPURenderPipeline> before = snapshot(cache, table.count());

    mat::HotReload hot;
    check(hot.start(wgsl_path), "the watcher is up on the library directory");
    std::printf("  watch: %s backend\n",
                hot.backend() == platform::WatchBackend::Native ? "native" : "polling");

    // 1. ВЕРНАЯ правка — позитивный контроль всему остальному.
    check(write_file(wgsl_path, wgsl + "\nfn hot_probe(x: f32) -> f32 { return x * 2.0; }\n"),
          "the valid edit is written");
    check(wait_event(hot, cache) == mat::ReloadEvent::Reloaded, "a valid edit reloads the library");
    check(cache.reloads() == 1, "the reload is counted once");
    check(cache.pipelines_created() == 6, "the reload rebuilt all three pipelines");
    const std::vector<WGPURenderPipeline> live = snapshot(cache, table.count());
    check(all_replaced(before, live), "every material now draws with a rebuilt pipeline");

    // 2. БИТАЯ правка — сцена обязана остаться прежней, а причина быть названной.
    check(write_file(wgsl_path, "let broken_here: f32 = ;\n" + wgsl), "the broken edit is written");
    check(wait_event(hot, cache) == mat::ReloadEvent::Rejected, "a broken edit is rejected");
    check(hot.rejects() == 1, "the rejection is counted once");
    check(cache.reloads() == 1, "a rejected edit does not count as a reload");
    check(cache.pipelines_created() == 6, "a rejected edit compiles nothing into the cache");
    check(base_name(hot.diag().file) == "sprite_effects.wgsl", "the diagnostic names the file");
    check(hot.diag().line > 0, "the diagnostic names the line");
    std::printf("  rejected: %s\n", mat::format_diag(hot.diag()).c_str());
    check(snapshot(cache, table.count()) == live, "the previous library is still the one drawing");
    check(cache.fallbacks() == 0, "a broken edit does not push materials onto the fallback");

    cache.shutdown();
    gpu.shutdown();
    std::printf("hot-reload: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
