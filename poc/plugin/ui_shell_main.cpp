#include "manifest.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow* win = glfwCreateWindow(1280, 800, "like-nes plugin UI-shell (panels from manifests)", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    bool layout_built = false;
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
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
        int w, h; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
