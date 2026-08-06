#include "editor_gate6.hpp"
#include "editor_gate6_session.hpp"
#include "editor_ui.hpp"
#include "capture.hpp"
#include "gpu.hpp"
#include "wgpu_imgui.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <utility>
#include <vector>

namespace ide::editor {
namespace {

constexpr WGPUColor kClear{0.08, 0.09, 0.11, 1.0};
constexpr int kFrames = 150, kDragStart = 20, kDragEnd = 70, kShot = 90, kUndo = 110;

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++failures;
}

// «Кадр отрисован» без этой проверки означало бы только «readback не упал»: чёрное окно вернуло бы
// столько же байт, сколько живой UI, и PNG-доказательство оказалось бы доказательством ничего.
bool frame_has_content(const std::vector<uint8_t>& px) {
    if (px.size() < 4) return false;
    size_t differing = 0;
    for (size_t i = 4; i + 3 < px.size(); i += 4)
        if (px[i] != px[0] || px[i + 1] != px[1] || px[i + 2] != px[2]) ++differing;
    return differing * 20 > px.size() / 4;   // > 5 % пикселей не равны первому
}

void shoot(const GpuContext& gpu, WGPUTextureFormat fmt, GLFWwindow* win, const char* path) {
    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    if (w <= 0 || h <= 0) { check(false, "frame captured to PNG (zero-sized framebuffer)"); return; }

    // Тот же draw-data, что уходит в окно, но в собственную текстуру: swapchain-текстуру читать
    // нельзя (она без CopySrc), а формат обязан совпадать с тем, которым инициализирован
    // ImGui_ImplWGPU, иначе валидация отвергнет пайплайн.
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    td.format = fmt;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    WGPUTexture tex = wgpuDeviceCreateTexture(gpu.device, &td);
    WGPUTextureView view = wgpuTextureCreateView(tex, nullptr);
    wgpu_imgui::draw_into(gpu, view, kClear);
    std::vector<uint8_t> px = capture::readback_rgba(gpu.device, gpu.queue, tex,
                                                     static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(tex);

    if (fmt == WGPUTextureFormat_BGRA8Unorm || fmt == WGPUTextureFormat_BGRA8UnormSrgb)
        for (size_t i = 0; i + 2 < px.size(); i += 4) std::swap(px[i], px[i + 2]);

    check(frame_has_content(px), "viewport drew a frame (not a flat fill)");
    check(!px.empty() && capture::write_png(path, px, static_cast<uint32_t>(w), static_cast<uint32_t>(h)),
          "frame captured to PNG");
    std::printf("  screenshot: %s (%dx%d)\n", path, w, h);
}

bool grid_shows(EditorState& st, uint64_t gid, int32_t raw_x) {
    for (const auto& r : build_property_grid(st.scene, gid))
        if (r.component == "Position" && r.member == "x")
            return r.value.rfind(std::to_string(raw_x), 0) == 0;
    return false;
}

} // namespace

int run_gate6(EditorState& st, GLFWwindow* win, const GpuContext& gpu, WGPUSurface surface,
              WGPUTextureFormat fmt, const char* out_png) {
    failures = 0;
    std::printf("\n=== gate 6 (spec #13): live editor run\n");
    if (!report_session(probe_session())) ++failures;
    WGPUAdapterProperties props = {};
    wgpuAdapterGetProperties(gpu.adapter, &props);
    std::printf("  adapter   : %s\n\n", props.name ? props.name : "?");

    const Position* sel = st.scene.get(st.sel).try_get<Position>();
    if (!sel) { std::printf("  FAIL scene has no selected entity - the scenario is impossible\n"); return 1; }
    const uint64_t gid = st.sel;
    const Position before = *sel;

    // Хит-тест гизмо в тех же координатах, где его рисует вьюпорт: это ровно та математика, по
    // которой мышь владельца попадёт в ось. Настоящий клик она не заменяет и не претендует.
    Gizmo g;
    world_to_screen(st.cam, static_cast<float>(before.x.to_double()),
                    static_cast<float>(before.y.to_double()), g.sx, g.sy);
    check(gizmo_hit(g, g.sx + g.len * 0.5f, g.sy) == GizmoPart::AxisX, "gizmo: axis X hit-tests where it is drawn");
    check(gizmo_hit(g, g.sx, g.sy - g.len * 0.5f) == GizmoPart::AxisY, "gizmo: axis Y hit-tests where it is drawn");
    check(gizmo_hit(g, g.sx + 4 * g.len, g.sy + 4 * g.len) == GizmoPart::None, "gizmo: away from the axes - a miss");

    Position dragged{}, undone{};
    size_t depth_after_drag = 0;
    bool grid_followed = false, built = false;
    int presented = 0;

    for (int f = 0; f < kFrames && !glfwWindowShouldClose(win); ++f) {
        glfwPollEvents();
        if (f == kDragStart) st.bus.begin_group();
        if (f > kDragStart && f <= kDragEnd)
            st.bus.set_component<Position>(gid, {fix32::from_raw(before.x.raw + (f - kDragStart) * 512), before.y});
        if (f == kDragEnd) {
            st.bus.end_group();
            dragged = *st.scene.get(gid).try_get<Position>();
            depth_after_drag = st.bus.undo_depth();
            grid_followed = grid_shows(st, gid, dragged.x.raw);
        }
        if (f == kUndo) { st.bus.undo(); undone = *st.scene.get(gid).try_get<Position>(); }

        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw_ui(st, built);
        ImGui::Render();

        // Кадр снимка не презентуется: draw-data рисуется один раз, и цель у него другая.
        if (f == kShot && out_png) shoot(gpu, fmt, win, out_png);
        else if (wgpu_imgui::present(gpu, surface, kClear)) ++presented;
    }

    check(presented > kFrames / 2, "frames reach the swapchain of this session");
    check(dragged.x.raw != before.x.raw, "gizmo drag moved the entity");
    check(depth_after_drag == 1, "drag = one undo transaction, not fifty");
    check(grid_followed, "inspector shows the new value (property grid from meta)");
    check(undone.x.raw == before.x.raw && undone.y.raw == before.y.raw, "undo restored the position");

    std::printf("\n=== gate 6: %s (failures: %d)\n", failures ? "FAIL" : "PASS", failures);
    std::printf("Left to the eye: the window IS visible on screen, and the gizmo obeys a real mouse -\n"
                "        the process cannot know that about itself. Attach the screenshot and this output.\n");
    return failures ? 1 : 0;
}

} // namespace ide::editor
