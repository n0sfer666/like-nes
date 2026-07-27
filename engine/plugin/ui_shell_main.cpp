#include "manifest.hpp"
#include "gpu.hpp"
#include "wgpu_imgui.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "platform_args.hpp"
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>
#include <cstdio>
#include <string>
#include <vector>

struct PanelRT {
    PanelDecl decl;
    bool open = true;
    std::vector<int> ints;
    std::vector<bool> bools;
};

static ImGuiID slot_node(DockSlot s, ImGuiID l, ImGuiID r, ImGuiID b, ImGuiID c) {
    switch (s) {
        case DockSlot::Left: return l;
        case DockSlot::Right: return r;
        case DockSlot::Bottom: return b;
        default: return c;
    }
}

static std::string window_key(const PanelDecl& d) { return d.title + "###" + d.id; }

static void draw_panel(PanelRT& p) {
    if (!p.open) return;
    if (ImGui::Begin(window_key(p.decl).c_str(), &p.open)) {
        int ii = 0, bi = 0;
        for (const auto& w : p.decl.widgets) {
            switch (w.kind) {
                case WidgetKind::Text: ImGui::TextUnformatted(w.label.c_str()); break;
                case WidgetKind::Button: ImGui::Button(w.label.c_str()); break;
                case WidgetKind::Checkbox: {
                    bool v = p.bools[bi];
                    if (ImGui::Checkbox(w.label.c_str(), &v)) p.bools[bi] = v;
                    ++bi; break;
                }
                case WidgetKind::SliderInt:
                    ImGui::SliderInt(w.label.c_str(), &p.ints[ii], w.min, w.max);
                    ++ii; break;
            }
        }
    }
    ImGui::End();
}

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::vector<PanelRT> panels;
    for (int i = 1; i < argc; ++i) {
        Manifest m = parse_manifest(argv[i]);
        if (!m.ok) { std::fprintf(stderr, "[shell] bad manifest %s: %s\n", argv[i], m.error.c_str()); continue; }
        std::printf("[shell] loaded manifest '%s' (%zu panels)\n", m.id.c_str(), m.panels.size());
        for (auto& pd : m.panels) {
            PanelRT rt; rt.decl = pd;
            for (const auto& w : pd.widgets) {
                if (w.kind == WidgetKind::SliderInt) rt.ints.push_back((w.min + w.max) / 2);
                if (w.kind == WidgetKind::Checkbox) rt.bools.push_back(false);
            }
            panels.push_back(std::move(rt));
        }
    }
    if (panels.empty()) { std::fprintf(stderr, "[shell] no panels\n"); return 2; }

    if (!glfwInit()) { std::fprintf(stderr, "[shell] glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // WebGPU-surface, без GL-контекста
    GLFWwindow* win = glfwCreateWindow(1280, 800, "like-nes plugin UI-shell (panels from manifests)", nullptr, nullptr);
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

    bool layout_built = false;
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        int cw = 0, ch = 0;
        glfwGetFramebufferSize(win, &cw, &ch);
        if (cw > 0 && ch > 0 && (cw != fbw || ch != fbh)) {   // resize → реконфиг surface
            fbw = cw; fbh = ch;
            wgpu_imgui::configure_surface(surface, gpu.adapter, gpu.device, (uint32_t)fbw, (uint32_t)fbh);
        }
        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowViewport(vp->ID);
        ImGuiWindowFlags host = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("DockHost", nullptr, host);
        ImGui::PopStyleVar(2);
        ImGuiID dock_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dock_id, ImVec2(0, 0), ImGuiDockNodeFlags_None);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                for (auto& p : panels)
                    ImGui::MenuItem((p.decl.title + "###mi_" + p.decl.id).c_str(), nullptr, &p.open);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();

        if (!layout_built) {
            layout_built = true;
            ImGui::DockBuilderRemoveNode(dock_id);
            ImGui::DockBuilderAddNode(dock_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock_id, vp->WorkSize);
            ImGuiID center = dock_id, l, r, b;
            l = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
            r = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, nullptr, &center);
            b = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);
            for (auto& p : panels)
                ImGui::DockBuilderDockWindow(window_key(p.decl).c_str(), slot_node(p.decl.dock, l, r, b, center));
            ImGui::DockBuilderFinish(dock_id);
        }

        for (auto& p : panels) draw_panel(p);

        ImGui::Render();
        wgpu_imgui::present(gpu, surface, WGPUColor{0.10, 0.11, 0.13, 1.0});
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
