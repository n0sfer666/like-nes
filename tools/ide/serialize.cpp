#include "serialize.hpp"
#include "../asset/hash.hpp"
#include <cstdlib>
#include <sstream>

namespace ide {
namespace {

template <typename T>
void emit(std::string& out, const flecs::world& w, flecs::entity e, const char* name) {
    const T* v = e.try_get<T>();
    if (!v) return;
    flecs::string js = w.to_json<T>(v);
    out += "C ";
    out += name;
    out += ' ';
    out += js.c_str();
    out += '\n';
}

template <typename T>
void set_from_json(flecs::world& w, flecs::entity e, const std::string& json) {
    T v{};
    w.from_json<T>(&v, json.c_str());
    e.set<T>(v);
}

void apply_component(flecs::world& w, flecs::entity e,
                     const std::string& name, const std::string& json) {
    if (name == "Name") set_from_json<Name>(w, e, json);
    else if (name == "Parent") set_from_json<Parent>(w, e, json);
    else if (name == "Position") set_from_json<Position>(w, e, json);
    else if (name == "Velocity") set_from_json<Velocity>(w, e, json);
}

} // namespace

std::string serialize(const Scene& s) {
    std::string out = "# like-nes scene v1\n";
    const flecs::world& w = s.world();
    for (const auto& [guid, e] : s.entities()) {
        out += "E ";
        out += std::to_string(guid);
        out += '\n';
        emit<Name>(out, w, e, "Name");
        emit<Parent>(out, w, e, "Parent");
        emit<Position>(out, w, e, "Position");
        emit<Velocity>(out, w, e, "Velocity");
    }
    return out;
}

void deserialize(Scene& s, const std::string& text) {
    s.clear();
    flecs::world& w = s.world();
    std::istringstream in(text);
    std::string line;
    flecs::entity cur;
    bool have_cur = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (line.starts_with("E ")) {
            const char* p = line.c_str() + 2;
            if (*p < '0' || *p > '9') continue;
            cur = s.create(std::strtoull(p, nullptr, 10));
            have_cur = true;
        } else if (line.starts_with("C ")) {
            if (!have_cur) continue;
            size_t sp = line.find(' ', 2);
            if (sp == std::string::npos) continue;
            std::string name = line.substr(2, sp - 2);
            std::string json = line.substr(sp + 1);
            apply_component(w, cur, name, json);
        }
    }
}

uint64_t golden_hash(const Scene& s) {
    std::string t = serialize(s);
    return asset::fnv1a(t.data(), t.size());
}

std::string serialize_entity(const Scene& s, uint64_t guid) {
    std::string out;
    if (!s.exists(guid)) return out;
    const flecs::world& w = s.world();
    flecs::entity e = s.get(guid);
    emit<Name>(out, w, e, "Name");
    emit<Parent>(out, w, e, "Parent");
    emit<Position>(out, w, e, "Position");
    emit<Velocity>(out, w, e, "Velocity");
    return out;
}

void restore_entity(Scene& s, uint64_t guid, const std::string& body) {
    flecs::entity e = s.create(guid);
    flecs::world& w = s.world();
    std::istringstream in(body);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.starts_with("C ")) continue;
        size_t sp = line.find(' ', 2);
        if (sp == std::string::npos) continue;
        apply_component(w, e, line.substr(2, sp - 2), line.substr(sp + 1));
    }
}

} // namespace ide
