#pragma once
#include "../scene.hpp"
#include <cstdint>
#include <string>
#include <vector>

// Property-grid из flecs meta (спека #7, гейт 6): генерик-обход полей ЛЮБОГО компонента сущности
// через рефлексию (EcsStruct member-list + meta-cursor значений), НЕ per-component hand-code. Третья
// опора ставки «одна рефлексия» (после save/load #гейт1 и IPC-зеркала #гейт3). fix32 opaque→raw,
// std::string opaque→строка — cursor обрабатывает прозрачно.
namespace ide::editor {

struct PropRow {
    std::string component;   // имя компонента (Position, Velocity, ...)
    std::string member;      // имя поля (x, y, value, guid)
    std::string kind;        // "fix32" | "i32" | "u64" | "f32" | "string" | ...
    std::string value;       // отформатированное значение
    bool has_range = false;  // range из meta (для слайдера min/max в UI)
    double range_min = 0, range_max = 0;
};

// Собрать property-grid для сущности guid: строки по всем её компонентам с meta (EcsStruct).
std::vector<PropRow> build_property_grid(const Scene& s, uint64_t guid);

} // namespace ide::editor
