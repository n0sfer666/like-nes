#include "editor_ui.hpp"
#include "editor_gate6.hpp"
#include "platform_args.hpp"
#include "gpu.hpp"
#include "wgpu_imgui.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>
#include <cstdio>
#include <string>

// Live editor-shell (спека #7, гейт 6): ImGui docking (#6) UI из editor_ui.hpp + рендер-бэкенд
// WebGPU (wgpu-native, как render #2 — НЕ deprecated OpenGL); оконная обвязка = render/wgpu_imgui.
// Owner-HW (окно). Состояние — ЛОКАЛИ main, НЕ глобалы (flecs::world в static-init → SIGSEGV).
//
// `--gate6 [out.png]` — тот же процесс, то же окно и тот же surface, но сценарий гейта прогоняется
// сам и возвращает код возврата: владельцу остаётся то, чего процесс о себе знать не может.
using namespace ide;
using namespace ide::editor;

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);   // путь снимка гейта приходит аргументом — шов обязателен

    bool gate6 = false;
    const char* gate6_png = "gate6.png";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) != "--gate6") continue;
        gate6 = true;
        if (i + 1 < argc && argv[i + 1][0] != '-') gate6_png = argv[++i];
    }

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

    int rc = 0;
    bool built = false;
    if (gate6) {
        rc = run_gate6(st, win, gpu, surface, fmt, gate6_png);
    } else {
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
    }

    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    wgpuSurfaceRelease(surface);
    gpu.shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return rc;
}
