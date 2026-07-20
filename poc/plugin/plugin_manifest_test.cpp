#include "manifest.hpp"
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: plugin_manifest_test <inspector.manifest> <console.manifest>\n");
        return 2;
    }
    Manifest a = parse_manifest(argv[1]);
    Manifest b = parse_manifest(argv[2]);

    std::printf("[manifest] %s: ok=%d id=%s api=%d panels=%zu\n",
                argv[1], a.ok, a.id.c_str(), a.api_version, a.panels.size());
    for (const auto& p : a.panels)
        std::printf("[manifest]   panel '%s' title=\"%s\" dock=%s widgets=%zu\n",
                    p.id.c_str(), p.title.c_str(), dock_name(p.dock), p.widgets.size());
    std::printf("[manifest] %s: ok=%d id=%s panels=%zu\n",
                argv[2], b.ok, b.id.c_str(), b.panels.size());

    bool a_ok = a.ok && a.id == "inspector" && a.api_version == 1 && a.panels.size() == 2;
    bool inspector_panel = !a.panels.empty()
                           && a.panels[0].id == "inspector"
                           && a.panels[0].title == "Inspector"
                           && a.panels[0].dock == DockSlot::Right
                           && a.panels[0].widgets.size() == 4;
    bool hierarchy_dock = a.panels.size() > 1 && a.panels[1].dock == DockSlot::Left;
    bool b_ok = b.ok && b.panels.size() == 2
                && b.panels[0].dock == DockSlot::Bottom
                && b.panels[1].dock == DockSlot::Center;

    bool robust_ok = true;
    if (argc > 3) {
        Manifest bad = parse_manifest(argv[3]);
        robust_ok = (bad.api_version == 0);
        std::printf("[manifest] %s: survived bad input (no terminate), api=%d panels=%zu\n",
                    argv[3], bad.api_version, bad.panels.size());
    }

    bool pass = a_ok && inspector_panel && hierarchy_dock && b_ok && robust_ok;
    std::printf("plugin-manifest: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
