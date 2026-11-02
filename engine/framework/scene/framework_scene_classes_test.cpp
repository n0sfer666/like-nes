#include "scene_read.hpp"

#include <array>
#include <cstdio>
#include <vector>

namespace {

using namespace framework;
using namespace framework::scene;
int failures = 0;
void check(bool ok, const char* why) {
    if (!ok) { ++failures; std::printf("  FAIL: %s\n", why); }
}

struct Lamps { std::vector<int32_t> values; };

bool lamp(const SceneObject& o, Lamps& out) {
    int32_t value = 0;
    if (!o.int_prop("value", value)) return false;
    out.values.push_back(value);
    return true;
}

constexpr ClassRule<Lamps> RULES[] = {{"lamp", tilemap::ObjectShape::Point, lamp}};
constexpr char WORDS[] = "lamp\0other\0name\0value\0";

struct Source {
    std::array<tilemap::MapObject, 2> objects{};
    std::array<tilemap::ObjectProp, 2> props{};
    Source() {
        for (uint32_t i = 0; i < objects.size(); ++i) {
            objects[i].id = i + 1;
            objects[i].name_offset = 11;
            objects[i].class_offset = i == 0 ? 0 : 5;
            objects[i].shape = static_cast<uint8_t>(tilemap::ObjectShape::Point);
            objects[i].first_prop = i;
            objects[i].prop_count = 1;
            props[i].name_offset = 16;
            props[i].type = static_cast<uint8_t>(tilemap::PropType::Int);
            props[i].value = static_cast<int32_t>(7 + i);
        }
    }
    tilemap::ObjectMap map() const { return {nullptr, objects, {}, props, WORDS}; }
};

using Reader = bool (*)(const tilemap::ObjectMap&, std::span<const ClassRule<Lamps>>, Lamps&, std::string&);

bool refuses_shape(Reader read) {
    Source s;
    s.objects[0].shape = static_cast<uint8_t>(tilemap::ObjectShape::Rect);
    Lamps out{{99}};
    std::string error;
    return !read(s.map(), RULES, out, error) && out.values.empty() && error.find("needs a point") != std::string::npos;
}

bool no_shape(const tilemap::ObjectMap& map, std::span<const ClassRule<Lamps>>, Lamps& out, std::string& error) {
    out = {};
    error.clear();
    return lamp(SceneObject(map, map.objects[0], error), out);
}

bool transactional(Reader read) {
    Source s;
    s.objects[1].class_offset = 0;
    s.props[1].type = static_cast<uint8_t>(tilemap::PropType::String);
    Lamps out{{99}};
    std::string error;
    return !read(s.map(), RULES, out, error) && out.values.empty() &&
           error.find("id 2") != std::string::npos && error.find("needs int") != std::string::npos;
}

bool partial(const tilemap::ObjectMap& map, std::span<const ClassRule<Lamps>>, Lamps& out, std::string& error) {
    out = {};
    for (const tilemap::MapObject& o : map.objects)
        if (!lamp(SceneObject(map, o, error), out)) return false;
    return true;
}

} // namespace

int main() {
    Source s;
    Lamps out{{99}};
    std::string error = "stale";
    check(read_objects<Lamps>(s.map(), RULES, out, error) && out.values == std::vector<int32_t>{7} && error.empty(),
          "a game-defined class is read, unknown classes are skipped, stale output and error are reset");
    check(refuses_shape(read_objects<Lamps>) && !refuses_shape(no_shape), "control: ignoring the class shape accepts an invalid object");
    check(transactional(read_objects<Lamps>) && !transactional(partial), "control: an in-place reader leaks partially read output on failure");
    const ClassRule<Lamps> missing[] = {{"lamp", tilemap::ObjectShape::Point, nullptr}};
    check(!read_objects<Lamps>(s.map(), missing, out, error) && error.find("no class reader") != std::string::npos,
          "an absent handler is refused without calling a null pointer");
    check(read_objects<Lamps>(s.map(), {}, out, error) && out.values.empty() && error.empty(), "an empty rule table recognizes no classes");
    std::printf("framework-scene-classes: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
