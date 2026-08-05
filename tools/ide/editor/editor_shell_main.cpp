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

    // Бэкенд окна glue создания поверхности выбирает на КОМПИЛЯЦИИ, а GLFW платформу — В РАНТАЙМЕ,
    // поэтому каталог сборки годен ровно для своей сессии. Расхождение до сих пор убивало процесс
    // ВНУТРИ wgpu и без слова о причине: X11-бинарь под Wayland — паникой «Display pointer is not
    // set», Wayland-бинарь под X11 — сегфолтом. Гейт 6 требует двух деревьев и гоняется руками, так
    // что перепутать каталог — норма, а не небрежность; здесь промах называет сам себя и говорит,
    // чем запускать. Проверка стоит ДО окна: создавать его незачем, ответ уже известен.
    //
    // Ветвление идёт по НАШИМ макросам из CMakeLists, а не по встроенному имени ОС: выбор ОС держит
    // сборка (инвариант 1 спеки #12), и вне Linux ни один из двух не определён — блока просто нет.
#if defined(LIKE_NES_GLFW_WAYLAND) || defined(LIKE_NES_GLFW_X11)
#  if defined(LIKE_NES_GLFW_WAYLAND)
    const int built_platform = GLFW_PLATFORM_WAYLAND;
    const char* built_for = "wayland";
    const char* right_binary = "build/editor_shell (каталог без -DLINUX_WAYLAND=ON)";
#  else
    const int built_platform = GLFW_PLATFORM_X11;
    const char* built_for = "x11";
    const char* right_binary = "build-way/editor_shell (каталог с -DLINUX_WAYLAND=ON)";
#  endif
    const int live_platform = glfwGetPlatform();
    if (live_platform != built_platform) {
        // Третьей платформы тут не ждём, но и молча звать её x11 нельзя: сообщение читают вместо
        // отладчика, и «x11» вместо null увело бы читателя не туда.
        const char* live_for = live_platform == GLFW_PLATFORM_WAYLAND ? "wayland"
                             : live_platform == GLFW_PLATFORM_X11     ? "x11"
                                                                      : "не x11 и не wayland";
        std::fprintf(stderr,
            "[editor] каталог сборки не для этой сессии: бинарь собран под %s,\n"
            "         а GLFW поднял платформу %s. Запускать надо %s.\n",
            built_for, live_for, right_binary);
        glfwTerminate();
        return 1;
    }
#endif

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
