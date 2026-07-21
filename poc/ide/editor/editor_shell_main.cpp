#include "../command.hpp"
#include "../scene.hpp"
#include "gizmo.hpp"
#include "property_grid.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <vector>

// Live editor-shell (спека #7, гейт 6): ImGui docking-shell (#6) + вьюпорт + гизмо (immediate
// debug-draw) + инспектор (property-grid из flecs meta) + иерархия (виртуализ.) + undo/redo.
// Owner-HW (окно). Data-путь валидируется headless в editor_selftest.
using namespace ide;
using namespace ide::editor;

namespace {

Scene g_scene;
CommandBus g_bus(g_scene);
std::vector<uint64_t> g_order;
uint64_t g_sel = 0;
Camera2D g_cam;

void seed() {
    for (int i = 0; i < 64; ++i) {
        uint64_t gid = 100 + i;
        auto e = g_scene.create(gid);
        e.set<Name>({std::string("entity_") + std::to_string(i)});
        e.set<Position>({fix32::from_int((i % 8) * 32 - 128), fix32::from_int((i / 8) * 32 - 128)});
        if (i % 3 == 0) e.set<Velocity>({fix32::from_raw(1000), fix32()});
        g_order.push_back(gid);
    }
    g_sel = g_order.empty() ? 0 : g_order[0];
    g_cam.zoom = 1.5f;
}

void hierarchy_panel() {
    if (ImGui::Begin("Hierarchy")) {
        ImGuiListClipper clip;   // виртуализация: рисуются только видимые строки
        clip.Begin(static_cast<int>(g_order.size()));
        while (clip.Step()) {
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                uint64_t gid = g_order[static_cast<size_t>(i)];
                const Name* nm = g_scene.get(gid).try_get<Name>();
                char label[64];
                std::snprintf(label, sizeof(label), "%s##%llu",
                              nm ? nm->value.c_str() : "?", static_cast<unsigned long long>(gid));
                if (ImGui::Selectable(label, g_sel == gid)) g_sel = gid;
            }
        }
    }
    ImGui::End();
}

void inspector_panel() {
    if (ImGui::Begin("Inspector")) {
        if (!g_scene.exists(g_sel)) { ImGui::TextUnformatted("no selection"); ImGui::End(); return; }
        ImGui::Text("entity %llu", static_cast<unsigned long long>(g_sel));
        ImGui::Separator();
        std::string cur_comp;
        for (const auto& r : build_property_grid(g_scene, g_sel)) {
            if (r.component != cur_comp) { cur_comp = r.component; ImGui::SeparatorText(cur_comp.c_str()); }
            ImGui::Text("%s [%s] = %s", r.member.c_str(), r.kind.c_str(), r.value.c_str());
        }
        ImGui::Separator();
        // Редактирование Position через command-bus (undo работает)
        if (const Position* p = g_scene.get(g_sel).try_get<Position>()) {
            int xy[2] = {p->x.raw, p->y.raw};
            bool changed = ImGui::DragInt2("Position.raw", xy, 256.0f);
            if (ImGui::IsItemActivated()) g_bus.begin_group();          // drag = 1 undo
            if (changed) g_bus.set_component<Position>(g_sel, {fix32::from_raw(xy[0]), fix32::from_raw(xy[1])});
            if (ImGui::IsItemDeactivated()) g_bus.end_group();
        }
        if (ImGui::Button("Undo")) g_bus.undo();
        ImGui::SameLine();
        if (ImGui::Button("Redo")) g_bus.redo();
    }
    ImGui::End();
}

void viewport_panel() {
    if (ImGui::Begin("Viewport")) {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::GetContentRegionAvail();
        g_cam.vw = sz.x; g_cam.vh = sz.y;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), IM_COL32(24, 26, 30, 255));

        // сетка
        for (int gx = -256; gx <= 256; gx += 64) {
            float sx, sy0, sy1;
            world_to_screen(g_cam, static_cast<float>(gx), -256, sx, sy0);
            world_to_screen(g_cam, static_cast<float>(gx), 256, sx, sy1);
            dl->AddLine(ImVec2(p0.x + sx, p0.y + sy0), ImVec2(p0.x + sx, p0.y + sy1), IM_COL32(40, 44, 50, 255));
        }
        // сущности (точки) + selection-outline + гизмо
        for (uint64_t gid : g_order) {
            const Position* p = g_scene.get(gid).try_get<Position>();
            if (!p) continue;
            float sx, sy;
            world_to_screen(g_cam, p->x.to_double(), p->y.to_double(), sx, sy);
            ImVec2 c(p0.x + sx, p0.y + sy);
            bool selp = (gid == g_sel);
            dl->AddCircleFilled(c, 4.0f, selp ? IM_COL32(255, 200, 60, 255) : IM_COL32(120, 160, 220, 255));
            if (selp) {
                dl->AddCircle(c, 9.0f, IM_COL32(255, 200, 60, 255), 0, 2.0f);   // outline
                Gizmo g; g.sx = sx; g.sy = sy;
                dl->AddLine(c, ImVec2(c.x + g.len, c.y), IM_COL32(230, 80, 80, 255), 2.0f);   // X
                dl->AddLine(c, ImVec2(c.x, c.y - g.len), IM_COL32(80, 200, 80, 255), 2.0f);   // Y
            }
        }
    }
    ImGui::End();
}

} // namespace

int main() {
    seed();

    if (!glfwInit()) { std::fprintf(stderr, "[editor] glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow* win = glfwCreateWindow(1400, 900, "like-nes IDE editor (viewport+gizmo+inspector)", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    bool built = false;
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowViewport(vp->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("DockHost", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
        ImGui::PopStyleVar(2);
        ImGuiID dock = ImGui::GetID("EditorDock");
        ImGui::DockSpace(dock, ImVec2(0, 0));
        ImGui::End();

        if (!built) {
            built = true;
            ImGui::DockBuilderRemoveNode(dock);
            ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock, vp->WorkSize);
            ImGuiID center = dock, l, r, b;
            l = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
            r = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, nullptr, &center);
            b = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.24f, nullptr, &center);
            ImGui::DockBuilderDockWindow("Hierarchy", l);
            ImGui::DockBuilderDockWindow("Inspector", r);
            ImGui::DockBuilderDockWindow("Console", b);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderFinish(dock);
        }

        hierarchy_panel();
        inspector_panel();
        viewport_panel();
        if (ImGui::Begin("Console")) ImGui::Text("undo depth: %zu", g_bus.undo_depth());
        ImGui::End();

        ImGui::Render();
        int w, h; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
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
