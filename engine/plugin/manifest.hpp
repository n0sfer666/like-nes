#pragma once
#include <string>
#include <vector>

enum class DockSlot { Left, Right, Bottom, Center };

enum class WidgetKind { Text, Button, Checkbox, SliderInt };

struct WidgetDecl {
    WidgetKind kind;
    std::string label;
    int min = 0;
    int max = 100;
};

struct PanelDecl {
    std::string id;
    std::string title;
    DockSlot dock = DockSlot::Center;
    std::vector<WidgetDecl> widgets;
};

struct Manifest {
    std::string id;
    std::string version;
    int api_version = 0;
    std::vector<PanelDecl> panels;
    bool ok = false;
    std::string error;
};

Manifest parse_manifest(const std::string& path);
const char* dock_name(DockSlot s);
