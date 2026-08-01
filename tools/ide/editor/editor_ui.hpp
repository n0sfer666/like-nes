#pragma once
#include "../command.hpp"
#include "../scene.hpp"
#include "gizmo.hpp"
#include "property_grid.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <string>
#include <vector>

// UI редактора (спека #7, гейт 6): состояние + панели ImGui (иерархия/инспектор/вьюпорт+гизмо/
// консоль) + докинг. Бэкенд-агностично (только ImGui API) — рендер-обвязка в editor_shell_main.cpp.
namespace ide::editor {

struct EditorState {
    Scene scene;
    CommandBus bus{scene};
    std::vector<uint64_t> order;
    uint64_t sel = 0;
    Camera2D cam;
};

inline void seed(EditorState& st) {
    for (int i = 0; i < 64; ++i) {
        uint64_t gid = 100 + i;
        auto e = st.scene.create(gid);
        e.set<Name>({std::string("entity_") + std::to_string(i)});
        e.set<Position>({fix32::from_int((i % 8) * 32 - 128), fix32::from_int((i / 8) * 32 - 128)});
        if (i % 3 == 0) e.set<Velocity>({fix32::from_raw(1000), fix32()});
        st.order.push_back(gid);
    }
    st.sel = st.order.empty() ? 0 : st.order[0];
    st.cam.zoom = 1.5f;
}

inline void hierarchy_panel(EditorState& st) {
    if (ImGui::Begin("Hierarchy")) {
        ImGuiListClipper clip;   // виртуализация: рисуются только видимые строки
        clip.Begin(static_cast<int>(st.order.size()));
        while (clip.Step())
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                uint64_t gid = st.order[static_cast<size_t>(i)];
                const Name* nm = st.scene.get(gid).try_get<Name>();
                char label[64];
                std::snprintf(label, sizeof(label), "%s##%llu",
                              nm ? nm->value.c_str() : "?", static_cast<unsigned long long>(gid));
                if (ImGui::Selectable(label, st.sel == gid)) st.sel = gid;
            }
    }
    ImGui::End();
}

inline void inspector_panel(EditorState& st) {
    if (ImGui::Begin("Inspector")) {
        if (!st.scene.exists(st.sel)) { ImGui::TextUnformatted("no selection"); ImGui::End(); return; }
        ImGui::Text("entity %llu", static_cast<unsigned long long>(st.sel));
        ImGui::Separator();
        std::string cur_comp;
        for (const auto& r : build_property_grid(st.scene, st.sel)) {
            if (r.component != cur_comp) { cur_comp = r.component; ImGui::SeparatorText(cur_comp.c_str()); }
            ImGui::Text("%s [%s] = %s", r.member.c_str(), r.kind.c_str(), r.value.c_str());
        }
        ImGui::Separator();
        if (const Position* p = st.scene.get(st.sel).try_get<Position>()) {
            int xy[2] = {p->x.raw, p->y.raw};
            bool changed = ImGui::DragInt2("Position.raw", xy, 256.0f);
            if (ImGui::IsItemActivated()) st.bus.begin_group();          // drag = 1 undo
            if (changed) st.bus.set_component<Position>(st.sel, {fix32::from_raw(xy[0]), fix32::from_raw(xy[1])});
            if (ImGui::IsItemDeactivated()) st.bus.end_group();
        }
        if (ImGui::Button("Undo")) st.bus.undo();
        ImGui::SameLine();
        if (ImGui::Button("Redo")) st.bus.redo();
    }
    ImGui::End();
}

inline void viewport_panel(EditorState& st) {
    if (ImGui::Begin("Viewport")) {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::GetContentRegionAvail();
        st.cam.vw = sz.x; st.cam.vh = sz.y;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), IM_COL32(24, 26, 30, 255));
        for (int gx = -256; gx <= 256; gx += 64) {
            float sx, sy0, sy1;
            world_to_screen(st.cam, static_cast<float>(gx), -256, sx, sy0);
            world_to_screen(st.cam, static_cast<float>(gx), 256, sx, sy1);
            dl->AddLine(ImVec2(p0.x + sx, p0.y + sy0), ImVec2(p0.x + sx, p0.y + sy1), IM_COL32(40, 44, 50, 255));
        }
        for (uint64_t gid : st.order) {
            const Position* p = st.scene.get(gid).try_get<Position>();
            if (!p) continue;
            float sx, sy;
            world_to_screen(st.cam, static_cast<float>(p->x.to_double()),
                            static_cast<float>(p->y.to_double()), sx, sy);
            ImVec2 c(p0.x + sx, p0.y + sy);
            bool selp = (gid == st.sel);
            dl->AddCircleFilled(c, 4.0f, selp ? IM_COL32(255, 200, 60, 255) : IM_COL32(120, 160, 220, 255));
            if (selp) {
                dl->AddCircle(c, 9.0f, IM_COL32(255, 200, 60, 255), 0, 2.0f);
                Gizmo g; g.sx = sx; g.sy = sy;
                dl->AddLine(c, ImVec2(c.x + g.len, c.y), IM_COL32(230, 80, 80, 255), 2.0f);
                dl->AddLine(c, ImVec2(c.x, c.y - g.len), IM_COL32(80, 200, 80, 255), 2.0f);
            }
        }
    }
    ImGui::End();
}

inline void build_dock(ImGuiID dock, ImGuiViewport* vp) {
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

inline void draw_ui(EditorState& st, bool& built) {
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
    if (!built) { built = true; build_dock(dock, vp); }

    hierarchy_panel(st);
    inspector_panel(st);
    viewport_panel(st);
    if (ImGui::Begin("Console")) ImGui::Text("undo depth: %zu", st.bus.undo_depth());
    ImGui::End();
}

} // namespace ide::editor
