// Запись сцены в текст. Разбор этого же текста живёт в `deserialize.cpp`: ответственности две, и
// делят они только заголовок и сам формат — у записи нет ни одного исхода, кроме успеха, а у
// разбора их четырнадцать (список — в `serialize.hpp`), и почти весь его объём — диагностика этих
// четырнадцати (аудит #21, A·2·8).
#include "serialize.hpp"
#include "../../engine/asset/hash.hpp"

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

} // namespace

std::string serialize(const Scene& s) {
    std::string out = std::string(SCENE_HEADER) + "\n";
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

} // namespace ide
