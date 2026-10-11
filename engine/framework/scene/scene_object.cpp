#include "scene_object.hpp"

namespace framework::scene {
namespace {

using tilemap::ObjectShape;
using tilemap::PropType;

const char* shape_word(uint8_t shape) {
    switch (shape) {
    case static_cast<uint8_t>(ObjectShape::Point): return "point";
    case static_cast<uint8_t>(ObjectShape::Rect): return "rectangle";
    default: return "polygon";
    }
}

const char* type_word(uint8_t type) {
    switch (type) {
    case static_cast<uint8_t>(PropType::String): return "string";
    case static_cast<uint8_t>(PropType::Int): return "int";
    case static_cast<uint8_t>(PropType::Fix): return "float";
    default: return "bool";
    }
}

fix32 raw(int32_t v) { return fix32::from_raw(v); }

fix32 end(int32_t at, int32_t size) { return fix32::from_raw(fix32::sat(int64_t{at} + size)); }

} // namespace

std::string SceneObject::who() const {
    return std::string(tilemap::object_text(map_, object_.class_offset)) + " '" +
           tilemap::object_text(map_, object_.name_offset) + "' (id " + std::to_string(object_.id) + ")";
}

bool SceneObject::fail(const std::string& why) const {
    error_ = who() + " " + why;
    return false;
}

bool SceneObject::shape(ObjectShape want) const {
    if (object_.shape == static_cast<uint8_t>(want)) return true;
    return fail(std::string("is a ") + shape_word(object_.shape) + "; class " +
                tilemap::object_text(map_, object_.class_offset) + " needs a " +
                shape_word(static_cast<uint8_t>(want)));
}

const tilemap::ObjectProp* SceneObject::typed(std::string_view name, PropType want, bool required, bool& ok) const {
    const tilemap::ObjectProp* p = tilemap::object_prop(map_, object_, name);
    const uint8_t w = static_cast<uint8_t>(want);
    ok = true;
    if (p == nullptr && required)
        ok = fail(std::string("needs ") + type_word(w) + " property '" + std::string(name) + "'");
    else if (p != nullptr && p->type != w)
        ok = fail("property '" + std::string(name) + "' is " + type_word(p->type) + "; class " +
                  tilemap::object_text(map_, object_.class_offset) + " needs " + type_word(w));
    return ok ? p : nullptr;
}

bool SceneObject::int_prop(std::string_view name, int32_t& out) const {
    bool ok = false;
    const tilemap::ObjectProp* p = typed(name, PropType::Int, true, ok);
    if (p != nullptr) out = p->value;
    return ok;
}

bool SceneObject::optional_int(std::string_view name, int32_t& out) const {
    bool ok = false;
    const tilemap::ObjectProp* p = typed(name, PropType::Int, false, ok);
    if (p != nullptr) out = p->value;
    return ok;
}

bool SceneObject::fix_prop(std::string_view name, fix32& out) const {
    bool ok = false;
    const tilemap::ObjectProp* p = typed(name, PropType::Fix, true, ok);
    if (p != nullptr) out = raw(p->value);
    return ok;
}

bool SceneObject::bool_prop(std::string_view name, bool& out) const {
    bool ok = false;
    const tilemap::ObjectProp* p = typed(name, PropType::Bool, true, ok);
    if (p != nullptr) out = p->value != 0;
    return ok;
}

bool SceneObject::text_prop(std::string_view name, std::string_view& out) const {
    bool ok = false;
    const tilemap::ObjectProp* p = typed(name, PropType::String, true, ok);
    if (p != nullptr) out = tilemap::object_text(map_, static_cast<uint32_t>(p->value));
    return ok;
}

bool SceneObject::word_prop(std::string_view name, std::initializer_list<std::string_view> words,
                            uint8_t& index) const {
    std::string_view text;
    if (!text_prop(name, text)) return false;
    std::string allowed;
    uint8_t i = 0;
    for (std::string_view w : words) {
        if (w == text) {
            index = i;
            return true;
        }
        allowed += (i++ == 0 ? "" : ", ") + std::string(w);
    }
    return fail("property '" + std::string(name) + "' is '" + std::string(text) + "'; allowed " + allowed);
}

bool SceneObject::at_least(std::string_view name, int32_t value, int32_t floor) const {
    if (value >= floor) return true;
    return fail("property '" + std::string(name) + "' is " + std::to_string(value) + "; allowed " +
                std::to_string(floor) + " or more");
}

Area SceneObject::area() const {
    return {&object_, {raw(object_.x_raw), raw(object_.y_raw), end(object_.x_raw, object_.w_raw),
                       end(object_.y_raw, object_.h_raw)}};
}

Mark SceneObject::mark() const { return {&object_, {raw(object_.x_raw), raw(object_.y_raw)}}; }

} // namespace framework::scene
