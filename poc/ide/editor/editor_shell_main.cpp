#include "editor_ui.hpp"
#include "gpu.hpp"
#include "wgpu_imgui.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>
#include <cstdio>

// Live editor-shell (спека #7, гейт 6): ImGui docking (#6) UI из editor_ui.hpp + рендер-бэкенд
// WebGPU (wgpu-native, как render #2 — НЕ deprecated OpenGL); оконная обвязка = render/wgpu_imgui.
// Owner-HW (окно). Состояние — ЛОКАЛИ main, НЕ глобалы (flecs::world в static-init → SIGSEGV).
using namespace ide;
using namespace ide::editor;

int main() {
    EditorState st;   // flecs-мир создаётся ЗДЕСЬ (не в static-init)
    seed(st);

    if (!glfwInit()) { std::fprintf(stderr, "[editor] glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // WebGPU-surface, без GL-контекста
    GLFWwindow* win = glfwCreateWindow(1400, 900, "like-nes IDE editor (WebGPU)", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }

    GpuContext gpu;
    gpu.instance = wgpuCreateInstance(nullptr);
    WGPUSurface surface = glfwGetWGPUSurface(gpu.instance, win);
    if (!gpu.init(surface)) {
        wgpuSurfaceRelease(surface); gpu.shutdown();
        glfwDestroyWindow(win); glfwTerminate(); return 1;
    }
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    WGPUTextureFormat fmt = wgpu_imgui::configure_surface(surface, gpu.adapter, gpu.device, (uint32_t)fbw, (uint32_t)fbh);

    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOther(win, true);
    ImGui_ImplWGPU_InitInfo info;
    info.Device = gpu.device;
    info.RenderTargetFormat = fmt;
    ImGui_ImplWGPU_Init(&info);

    bool built = false;
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        int w = 0, h = 0;
        glfwGetFramebufferSize(win, &w, &h);
        if (w > 0 && h > 0 && (w != fbw || h != fbh)) {   // resize → реконфиг surface
            fbw = w; fbh = h;
            wgpu_imgui::configure_surface(surface, gpu.adapter, gpu.device, (uint32_t)fbw, (uint32_t)fbh);
        }

        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw_ui(st, built);
        ImGui::Render();
        wgpu_imgui::present(gpu, surface, WGPUColor{0.08, 0.09, 0.11, 1.0});
    }

    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    wgpuSurfaceRelease(surface);
    gpu.shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
