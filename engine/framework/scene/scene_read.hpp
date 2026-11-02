#pragma once
#include <string>
#include <span>
#include <string_view>
#include <utility>

#include "object_read.hpp"
#include "scene_object.hpp"

namespace framework::scene {

template <class Out>
struct ClassRule {
    std::string_view cls;
    tilemap::ObjectShape shape;
    bool (*read)(const SceneObject&, Out&);
};

// Неизвестные классы пропускаются. При отказе наружу не попадает частично прочитанный результат.
template <class Out>
bool read_objects(const tilemap::ObjectMap& map, std::span<const ClassRule<Out>> rules,
                  Out& out, std::string& error) {
    out = Out{};
    error.clear();
    Out pending{};
    for (const tilemap::MapObject& object : map.objects) {
        const std::string_view cls = tilemap::object_text(map, object.class_offset);
        for (const ClassRule<Out>& rule : rules) {
            if (rule.cls != cls) continue;
            const SceneObject view(map, object, error);
            if (rule.read == nullptr) return view.fail("has no class reader");
            if (!view.shape(rule.shape) || !rule.read(view, pending)) return false;
            break;
        }
    }
    out = std::move(pending);
    return true;
}

} // namespace framework::scene
