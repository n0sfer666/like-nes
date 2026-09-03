#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cache.hpp"
#include "capture.hpp"
#include "gpu.hpp"
#include "light_checks.hpp"
#include "light_frame.hpp"
#include "light_pass.hpp"
#include "material_frame.hpp"
#include "material_scene.hpp"
#include "shadow_checks.hpp"
#include "slot_pass.hpp"
#include "slot_textures.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"

// Заголовки двух подсистем зовутся ОДИНАКОВО (`bake.hpp`, `table.hpp`), и порядок -I решал бы,
// чей из них попадёт в этот TU. Путь здесь явный — иначе перестановка целей в CMake молча
// подменяет таблицу.
#include "../light/bake.hpp"
#include "../light/table.hpp"
#include "../material/bake.hpp"
#include "../material/table.hpp"

// Гейт 7 спеки #18 (вертикаль 3): свет — ДАННЫЕ. Сцена материалов рисуется в текстуру, а проход
// освещения читает источники из таблицы, число которых берёт `arrayLength` — в шейдере его нет.
//
// Форма та же, что у гейта 1: эталонный PNG сверяет владелец на Metal, на чужих ОС идёт
// `--selftest` (те же утверждения плюс совпадение двух прогонов байт в байт).
namespace {

constexpr uint32_t W = 640, H = 360;
constexpr double PIXEL_EPS = 0.02;
constexpr double FRAC_TOL = 0.01;
constexpr double MAX_CAP = 0.35;

using lightgold::check;

} // namespace

int main(int argc, char** argv) {
    // Небуферизованный stdout по той же улике, что и в material_golden: оборвавшийся прогон
    // обязан оставить свои строки, иначе первый же греп обвинит не то.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    platform::Args utf8_argv(argc, argv);
    std::string mdir = "engine/material/library";
    std::string ldir = "engine/light/library";
    const char* golden_path = nullptr;
    bool update = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--library") == 0 && i + 1 < argc) mdir = argv[++i];
        else if (std::strcmp(argv[i], "--lights") == 0 && i + 1 < argc) ldir = argv[++i];
        else if (std::strcmp(argv[i], "--golden") == 0 && i + 1 < argc) golden_path = argv[++i];
        else if (std::strcmp(argv[i], "--update") == 0) update = true;
        else if (std::strcmp(argv[i], "--selftest") == 0) golden_path = nullptr;
    }

    std::string msrc, wgsl, lsrc;
    if (!platform::read_text(mdir + "/library.mat", msrc) ||
        !platform::read_text(mdir + "/sprite_effects.wgsl", wgsl) ||
        !platform::read_text(ldir + "/lights.txt", lsrc)) {
        std::printf("light-gpu: FAIL (library not readable under %s / %s)\n", mdir.c_str(),
                    ldir.c_str());
        return 1;
    }
    std::vector<uint8_t> mbytes;
    mat::BakeError merr;
    if (!mat::bake_materials(msrc, mbytes, merr)) {
        std::printf("light-gpu: FAIL (library.mat:%d: %s)\n", merr.line, merr.message.c_str());
        return 1;
    }
    mat::Table mtable;
    if (mtable.load(mbytes.data(), mbytes.size()) != mat::LoadResult::Ok) {
        std::printf("light-gpu: FAIL (material table did not open)\n");
        return 1;
    }
    std::vector<uint8_t> lbytes;
    light::Table ltable;
    if (!lightgold::load_table(lsrc, lbytes, ltable, "lights.txt")) {
        std::printf("light-gpu: FAIL (light table did not open)\n");
        return 1;
    }

    GpuContext gpu;
    if (!gpu.init(nullptr)) {
        std::printf("light-gpu: FAIL (no adapter)\n");
        gpu.shutdown();
        return 1;
    }
    mat::CacheDesc cd;
    cd.device = gpu.device; cd.queue = gpu.queue; cd.table = &mtable; cd.wgsl = wgsl.c_str();
    mat::Cache cache;
    if (!cache.init(cd)) {
        std::printf("light-gpu: FAIL (cache did not start)\n");
        gpu.shutdown();
        return 1;
    }
    cache.warm_up();
    matgold::Scene scene;
    check(scene.init(gpu.device, gpu.queue, W, H, cache.layout()), "scene resources are ready");
    scene.build(mtable, cache);

    lightgfx::Pass pass;
    const float aspect = static_cast<float>(W) / static_cast<float>(H);
    check(pass.init(gpu.device, gpu.queue, ltable, WGPUTextureFormat_RGBA8Unorm, aspect),
          "the lighting pass starts over the shipped light table");
    std::printf("  lights: %u source(s) in the table, %u uploaded\n", ltable.count(),
                pass.lights());
    check(pass.lights() == ltable.count(), "every source of the table reaches the GPU");
    // Ни три (столько зашито в PoC-сцене), ни шесть (столько влезало в прежний юниформ): совпади
    // число с любым из них, кадр «из таблицы» был бы неотличим от кадра «как было».
    check(ltable.count() != 3 && ltable.count() != 6,
          "the shipped set has neither the hardcoded three nor the uniform-sized six");

    // Нормали спрайтов (шаг B): карту выбирает ТЕКСТУРНЫЙ СЛОТ материала, банк отдаёт её по guid
    // ассета — ни номер слота, ни имя карты в проходе не зашиты.
    slotgold::Bank bank;
    check(bank.init(gpu.device, gpu.queue), "the slot-texture bank is ready");
    slotgfx::Pass normals;
    check(normals.init(gpu.device, gpu.queue, mtable, bank, slotgfx::normals_of(bank), W, H,
                       scene.albedo_view(), WGPUTextureFormat_RGBA8Unorm),
          "the normal pass starts over the shipped material table");
    normals.build(scene);
    // Перекрыватели (шаг C) — ТОТ ЖЕ класс прохода с другим `Desc`: имя слота, точка входа, цвет
    // очистки и запасная карта. Вторая реализация разъехалась бы с первой ровно в том, что гейт и
    // проверяет, — в том, откуда берётся карта.
    slotgfx::Pass occluders;
    check(occluders.init(gpu.device, gpu.queue, mtable, bank, slotgfx::occluders_of(bank), W, H,
                         scene.albedo_view(), WGPUTextureFormat_RGBA8Unorm),
          "the occluder pass starts over the shipped material table");
    occluders.build(scene);

    uint32_t draws = 0, mdraws = 0;
    const std::vector<uint8_t> off =
        lightgold::render_frame(gpu, scene, lightgold::Graph{nullptr, &normals, &occluders}, W, H,
                                draws);
    const std::vector<uint8_t> mat_px = matgold::render_frame(gpu, scene, W, H, mdraws);
    check(!off.empty() && off == mat_px,
          "a switched-off pass returns the previous frame byte for byte");
    check(draws == mdraws, "the graph without the pass draws the scene in the same calls");

    const lightgold::Graph graph{&pass, &normals, &occluders};
    const std::vector<uint8_t> lit = lightgold::render_frame(gpu, scene, graph, W, H, draws);
    check(!lit.empty() && lit != off, "the pass changes the frame it is given");
    std::printf("  frame: %u draw call(s) into the albedo target\n", draws);

    lightgold::count_comes_from_data(gpu, scene, W, H);
    lightgold::normals_come_from_slots(gpu, scene, mtable, pass, normals, W, H);
    lightgold::shadows_come_from_slots(gpu, scene, mtable, pass, normals, occluders, W, H);
    lightgold::softness_comes_from_the_table(gpu, scene, normals, occluders, W, H);
    lightgold::report_cost(gpu, scene, pass, normals, occluders, W, H);

    if (golden_path) {
        std::vector<uint8_t> ref;
        uint32_t rw = 0, rh = 0;
        if (update || !capture::read_png(golden_path, ref, rw, rh)) {
            check(capture::write_png(golden_path, lit, W, H), "reference written");
            std::printf("  golden %s: %s\n", update ? "updated" : "written", golden_path);
        } else {
            const capture::DiffResult diff =
                capture::compare(lit, ref, PIXEL_EPS, FRAC_TOL, MAX_CAP);
            std::printf("  golden %s: mean=%.5f max=%.5f frac=%.5f\n", golden_path, diff.mean_abs,
                        diff.max_abs, diff.frac_over);
            check(rw == W && rh == H, "reference has the frame size");
            check(diff.pass, "frame agrees with the reference");
        }
    } else {
        // Повтор на ОДНОМ бэкенде обязан совпасть ТОЧНО: допуск для чужой видеокарты спрятал бы
        // собственный дрейф тракта, а он и есть то, что видно без эталонного файла.
        uint32_t again = 0;
        const std::vector<uint8_t> lit2 = lightgold::render_frame(gpu, scene, graph, W, H, again);
        check(lit == lit2, "two runs on one backend give the same lit frame");
    }

    occluders.shutdown();
    normals.shutdown();
    bank.shutdown();
    pass.shutdown();
    scene.shutdown();
    cache.shutdown();
    gpu.shutdown();
    const int fails = lightgold::failures();
    std::printf("light-gpu: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
