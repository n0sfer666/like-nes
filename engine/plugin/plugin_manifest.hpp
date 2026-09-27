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
    // Версия ABI, объявленная файлом. Её сверяет `parse_manifest`, и манифест чужой версии не
    // становится `ok` — иначе поле было бы тем, чем оно и было до аудита #21 (A·2·7): числом,
    // которое читатель видит и принимает за работающую проверку. `0` здесь не значит «версия 0»:
    // столько же остаётся у манифеста без поля, у нечислового `api=` и у значения, не влезшего в
    // `int`, и различает их `error`, а не это число.
    int api_version = 0;
    std::vector<PanelDecl> panels;
    bool ok = false;
    std::string error;
};

Manifest parse_manifest(const std::string& path);
const char* dock_name(DockSlot s);
