#include "material_panel_gate.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "../compile/diagnostics.hpp"
#include "material_panel.hpp"
#include "platform_fs.hpp"

namespace ide::editor {
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

// Правка, которая ОБЯЗАНА поменять пиксели: неиспользуемая функция собралась бы так же, и
// «картинка изменилась» перестало бы отличать перерисовку от её отсутствия.
const char* FLASH_ORIG = "    return vec4f(mix(c.rgb, in.p0.rgb, in.p1.x * in.p0.a), c.a);";
const char* FLASH_LOUD = "    return vec4f(1.0, 0.0, 1.0, 1.0);";

bool wait_for(MaterialPanel& panel, uint32_t& reloads, uint32_t& rejects) {
    for (int i = 0; i < 200; ++i) {
        panel.poll();
        if (panel.reloads() != reloads || panel.rejects() != rejects) {
            reloads = panel.reloads();
            rejects = panel.rejects();
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

} // namespace

int run_gate3(GpuContext& gpu, const std::string& library_dir, const std::string& work) {
    // Правится КОПИЯ: гейт, редактирующий библиотеку в дереве, оставляет её битой ровно тогда,
    // когда падает сам, — и следующий прогон обвиняет в этом движок.
    std::string mat_src, wgsl;
    if (!platform::ensure_dir(work) ||
        !platform::read_text(library_dir + "/library.mat", mat_src) ||
        !platform::read_text(library_dir + "/sprite_effects.wgsl", wgsl)) {
        std::printf("gate 3: FAIL (library not readable under %s)\n", library_dir.c_str());
        return 1;
    }
    if (!write_file(work + "/library.mat", mat_src) ||
        !write_file(work + "/sprite_effects.wgsl", wgsl)) {
        std::printf("gate 3: FAIL (work copy not writable under %s)\n", work.c_str());
        return 1;
    }
    const std::string wgsl_path = work + "/sprite_effects.wgsl";

    MaterialPanel panel;
    if (!panel.init(gpu, work)) {
        std::printf("gate 3: FAIL (%s)\n", panel.status());
        return 1;
    }
    const uint64_t first = panel.preview_hash();
    check(first != 0, "the panel drew a preview before any edit");
    uint32_t reloads = panel.reloads(), rejects = panel.rejects();

    const std::size_t at = wgsl.find(FLASH_ORIG);
    check(at != std::string::npos, "the flash entry point is where the edit expects it");
    if (at == std::string::npos) { panel.shutdown(); return 1; }

    // 1. ВЕРНАЯ правка — позитивный контроль всему остальному.
    std::string edited = wgsl;
    edited.replace(at, std::string(FLASH_ORIG).size(), FLASH_LOUD);
    check(write_file(wgsl_path, edited), "the valid edit is written");
    check(wait_for(panel, reloads, rejects), "the panel noticed the valid edit");
    check(panel.reloads() == 1 && panel.rejects() == 0, "the valid edit is a reload, not a refusal");
    const uint64_t after = panel.preview_hash();
    check(after != first, "the preview was redrawn with the new library");

    // 2. БИТАЯ правка — на экране обязан остаться ПРЕЖНИЙ кадр, а причина быть названной.
    check(write_file(wgsl_path, "let broken_here: f32 = ;\n" + edited), "the broken edit is written");
    check(wait_for(panel, reloads, rejects), "the panel noticed the broken edit");
    check(panel.rejects() == 1 && panel.reloads() == 1, "the broken edit is refused, not applied");
    check(panel.preview_hash() == after, "the previous variant is still the picture on screen");

    const std::vector<build::Diagnostic> shown =
        build::parse_diagnostics(std::string(panel.diag()) + "\n");
    check(shown.size() == 1, "the panel line is one diagnostic for the panel parser");
    if (shown.size() == 1) {
        check(shown[0].line > 0, "the panel names the line of the broken edit");
        check(shown[0].file.find("sprite_effects.wgsl") != std::string::npos,
              "the panel names the file of the broken edit");
    }
    std::printf("  rejected: %s\n", panel.diag());

    panel.shutdown();
    std::printf("gate 3: %s (failures: %d)\n", fails == 0 ? "PASS" : "FAIL", fails);
    return fails == 0 ? 0 : 1;
}

} // namespace ide::editor
