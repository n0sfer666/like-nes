#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bake.hpp"
#include "cache.hpp"
#include "capture.hpp"
#include "gpu.hpp"
#include "material_frame.hpp"
#include "material_scene.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "table.hpp"

// Первый потребитель материалов (решение 6 спеки #18 — «голден первым»): сцена библиотеки,
// нарисованная ЧЕРЕЗ кэш «материал → пайплайн».
//
// Утверждений четыре, и три из них — числа, а не картинка. Кадр говорит «похоже на эталон» и молчит
// про то, во сколько вызовов он собран и сколько пайплайнов при этом создано, а гейты 4, 5 и 6
// спрашивают именно это. Числа портируемы: они одинаковы на Metal, lavapipe и WARP, поэтому
// `--selftest` — гейт CI на трёх ОС, а сверка с эталонным PNG остаётся у владельца.
namespace {

constexpr uint32_t W = 640, H = 360;
constexpr uint32_t PIPELINES = 3;   // три точки входа на семь материалов — решение 2 спеки
constexpr double PIXEL_EPS = 0.02;
constexpr double FRAC_TOL = 0.01;
constexpr double MAX_CAP = 0.35;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

// Через платформенный шов: `fopen` на MSVC депрекирован, и `/W4 /WX` раннера валит на нём сборку.
bool read_file(const std::string& path, std::string& out) { return platform::read_text(path, out); }

// Гейт 4: битый материал не роняет прогон и не отдаёт пустоту. Проверяется на СВОЁМ кэше, потому
// что утверждение о нуле запасных в кэше библиотеки — половина этого гейта: без него «отдали
// запасной» неотличимо от «всё всегда запасное».
void broken_material(GpuContext& gpu, const std::string& wgsl) {
    const std::string src =
        "material | broken | fs_nope | alpha\n"
        "param | tint | color | raw | 1, 1, 1, 1\n";
    std::vector<uint8_t> bytes;
    mat::BakeError err;
    if (!mat::bake_materials(src, bytes, err)) {
        check(false, "the broken-material fixture does not even bake");
        return;
    }
    mat::Table t;
    if (t.load(bytes.data(), bytes.size()) != mat::LoadResult::Ok) {
        check(false, "the broken-material fixture does not open");
        return;
    }
    mat::CacheDesc d;
    d.device = gpu.device; d.queue = gpu.queue; d.table = &t; d.wgsl = wgsl.c_str();
    mat::Cache c;
    check(c.init(d), "a cache over a broken library still starts");
    check(c.pipeline(0) != nullptr, "a broken material still draws with something");
    check(c.fallbacks() == 1, "the broken material is counted as a fallback");
    check(c.pipelines_created() == 0, "a missing entry point compiles nothing");
    c.shutdown();
}

} // namespace

int main(int argc, char** argv) {
    // Вывод гейта не буферизуется. Пайп делает stdout полностью буферизованным, и весь отчёт
    // прогона существует только при штатном выходе: прогон 33663261501 на windows-latest оборвался
    // между сообщением в stderr и сбросом буфера, гейт не увидел НИ ОДНОЙ строки и назвал это
    // «WARM-UP COUNT CHANGED» — обвинение не тому. Улика дороже нескольких записей в пайп.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    platform::Args utf8_argv(argc, argv);
    std::string dir = "engine/material/library";
    const char* golden_path = nullptr;
    bool update = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--library") == 0 && i + 1 < argc) dir = argv[++i];
        else if (std::strcmp(argv[i], "--golden") == 0 && i + 1 < argc) golden_path = argv[++i];
        else if (std::strcmp(argv[i], "--update") == 0) update = true;
        else if (std::strcmp(argv[i], "--selftest") == 0) golden_path = nullptr;
    }

    std::string src, wgsl;
    if (!read_file(dir + "/library.mat", src) || !read_file(dir + "/sprite_effects.wgsl", wgsl)) {
        std::printf("material-gpu: FAIL (library not readable under %s)\n", dir.c_str());
        return 1;
    }
    std::vector<uint8_t> bytes;
    mat::BakeError err;
    if (!mat::bake_materials(src, bytes, err)) {
        std::printf("material-gpu: FAIL (library.mat:%d: %s)\n", err.line, err.message.c_str());
        return 1;
    }
    mat::Table table;
    const mat::LoadResult lr = table.load(bytes.data(), bytes.size());
    if (lr != mat::LoadResult::Ok) {
        std::printf("material-gpu: FAIL (%s)\n", mat::load_reason(lr));
        return 1;
    }

    GpuContext gpu;
    if (!gpu.init(nullptr)) {
        std::printf("material-gpu: FAIL (no adapter)\n");
        gpu.shutdown();
        return 1;
    }

    mat::CacheDesc d;
    d.device = gpu.device; d.queue = gpu.queue; d.table = &table; d.wgsl = wgsl.c_str();
    mat::Cache cache;
    if (!cache.init(d)) {
        std::printf("material-gpu: FAIL (cache did not start)\n");
        gpu.shutdown();
        return 1;
    }
    cache.warm_up();
    std::printf("  warm-up: %u pipeline(s) for %u material(s), %u fallback(s)\n",
                cache.pipelines_created(), table.count(), cache.fallbacks());
    check(cache.pipelines_created() == PIPELINES, "the library warms up into three pipelines");
    check(cache.fallbacks() == 0, "nothing in the library falls back");
    const uint32_t after_warm = cache.pipelines_created();

    matgold::Scene scene;
    check(scene.init(gpu.device, gpu.queue, W, H, cache.layout()), "scene resources are ready");
    scene.build(table, cache);

    uint32_t draws = 0;
    std::vector<uint8_t> px = matgold::render_frame(gpu, scene, W, H, draws);
    std::printf("  frame: %u instance(s) in %u draw call(s)\n", scene.instances(), draws);
    check(!px.empty(), "the frame comes back from the GPU");
    // Позитивный контроль числам: три вызова отрисовки, каждый из которых ничего не нарисовал, дают
    // ровно тот же счётчик, что и рабочий кадр. Поэтому кадр обязан ОТЛИЧАТЬСЯ от заливки.
    std::size_t painted = 0;
    for (std::size_t i = 0; i + 3 < px.size(); i += 4) {
        if (px[i] != 13 || px[i + 1] != 15 || px[i + 2] != 23) ++painted;
    }
    std::printf("  painted: %.1f%% of the frame\n", 100.0 * static_cast<double>(painted) /
                                                    static_cast<double>(W * H));
    check(painted > W * H / 20, "the draw calls actually put pixels on the frame");
    check(scene.instances() == table.count() * matgold::PER_MATERIAL,
          "every material is drawn several times with different parameter values");
    check(draws == PIPELINES, "differing parameters do not split the batch");
    check(cache.pipelines_created() == after_warm, "a frame after warm-up compiles nothing");

    broken_material(gpu, wgsl);

    if (golden_path) {
        std::vector<uint8_t> ref;
        uint32_t rw = 0, rh = 0;
        if (update || !capture::read_png(golden_path, ref, rw, rh)) {
            check(capture::write_png(golden_path, px, W, H), "reference written");
            std::printf("  golden %s: %s\n", update ? "updated" : "written", golden_path);
        } else {
            const capture::DiffResult diff = capture::compare(px, ref, PIXEL_EPS, FRAC_TOL, MAX_CAP);
            std::printf("  golden %s: mean=%.5f max=%.5f frac=%.5f\n", golden_path, diff.mean_abs,
                        diff.max_abs, diff.frac_over);
            check(rw == W && rh == H, "reference has the frame size");
            check(diff.pass, "frame agrees with the reference");
        }
    } else {
        // Повтор на ОДНОМ бэкенде обязан совпасть ТОЧНО: допуск для чужой видеокарты здесь спрятал
        // бы собственный дрейф тракта, а он и есть то, что видно без эталонного файла.
        uint32_t again = 0;
        const std::vector<uint8_t> px2 = matgold::render_frame(gpu, scene, W, H, again);
        check(px == px2, "two runs on one backend give the same frame");
        check(again == draws, "the second frame is drawn in the same number of calls");
    }

    scene.shutdown();
    cache.shutdown();
    gpu.shutdown();
    std::printf("material-gpu: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
