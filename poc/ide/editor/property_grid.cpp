#include "property_grid.hpp"
#include <cstdio>
#include <string>

namespace ide::editor {
namespace {

// Определить kind поля: примитив напрямую, либо opaque → его as_type. Возвращает kind +
// признак opaque (для fix32=I32-opaque и std::string=String-opaque storage отличается от примитива).
ecs_primitive_kind_t member_kind(const ecs_world_t* w, ecs_entity_t mt, bool& is_opaque) {
    is_opaque = false;
    if (const EcsPrimitive* p = ecs_get(w, mt, EcsPrimitive)) return p->kind;
    if (const EcsOpaque* o = ecs_get(w, mt, EcsOpaque)) {
        if (const EcsPrimitive* ap = ecs_get(w, o->as_type, EcsPrimitive)) {
            is_opaque = true;
            return ap->kind;
        }
        // opaque с непримитивным as_type — не поддержано (не трактовать молча как fix32); follow-up
    }
    return EcsId;   // sentinel → default-ветка read_value ("?")
}

// Прямое чтение значения по offset (детерм.). fix32 (opaque I32) = int32 raw; std::string
// (opaque String) = std::string; примитивы — по kind. NB: генерик-opaque произвольного C++-типа
// (не fix32/std::string) — follow-up (через serialize-callback); cursor.get_int для I32-opaque=0.
void read_value(const void* base, int32_t offset, ecs_primitive_kind_t k, bool is_opaque,
                std::string& kind, std::string& value) {
    const char* p = static_cast<const char*>(base) + offset;
    char buf[80];
    if (is_opaque && k == EcsI32) {   // fix32
        int32_t raw = *reinterpret_cast<const int32_t*>(p);
        std::snprintf(buf, sizeof(buf), "%d (%.4f)", raw, static_cast<double>(raw) / 65536.0);
        kind = "fix32";
        value = buf;
        return;
    }
    if (is_opaque && k == EcsString) {   // std::string opaque
        kind = "string";
        value = *reinterpret_cast<const std::string*>(p);
        return;
    }
    switch (k) {
        case EcsBool: kind = "bool"; value = *reinterpret_cast<const bool*>(p) ? "true" : "false"; return;
        case EcsU8:  kind = "u8";  std::snprintf(buf, sizeof(buf), "%u", *reinterpret_cast<const uint8_t*>(p)); break;
        case EcsU16: kind = "u16"; std::snprintf(buf, sizeof(buf), "%u", *reinterpret_cast<const uint16_t*>(p)); break;
        case EcsU32: kind = "u32"; std::snprintf(buf, sizeof(buf), "%u", *reinterpret_cast<const uint32_t*>(p)); break;
        case EcsU64: kind = "u64"; std::snprintf(buf, sizeof(buf), "%llu",
                     static_cast<unsigned long long>(*reinterpret_cast<const uint64_t*>(p))); break;
        case EcsI8:  kind = "i8";  std::snprintf(buf, sizeof(buf), "%d", *reinterpret_cast<const int8_t*>(p)); break;
        case EcsI16: kind = "i16"; std::snprintf(buf, sizeof(buf), "%d", *reinterpret_cast<const int16_t*>(p)); break;
        case EcsI32: kind = "i32"; std::snprintf(buf, sizeof(buf), "%d", *reinterpret_cast<const int32_t*>(p)); break;
        case EcsI64: kind = "i64"; std::snprintf(buf, sizeof(buf), "%lld",
                     static_cast<long long>(*reinterpret_cast<const int64_t*>(p))); break;
        case EcsF32: kind = "f32"; std::snprintf(buf, sizeof(buf), "%.4f", *reinterpret_cast<const float*>(p)); break;
        case EcsF64: kind = "f64"; std::snprintf(buf, sizeof(buf), "%.4f", *reinterpret_cast<const double*>(p)); break;
        case EcsString: kind = "string"; { const char* s = *reinterpret_cast<const char* const*>(p); value = s ? s : ""; } return;
        default: kind = "?"; value = "?"; return;
    }
    value = buf;
}

} // namespace

std::vector<PropRow> build_property_grid(const Scene& s, uint64_t guid) {
    std::vector<PropRow> rows;
    if (!s.exists(guid)) return rows;
    flecs::entity e = s.get(guid);
    const ecs_world_t* w = s.world().c_ptr();

    const ecs_type_t* type = ecs_get_type(w, e);
    if (!type) return rows;

    for (int i = 0; i < type->count; ++i) {
        ecs_id_t id = type->array[i];
        if (id != (id & ECS_COMPONENT_MASK)) continue;   // только компоненты (не пары/флаги)
        ecs_entity_t comp = static_cast<ecs_entity_t>(id);
        const EcsStruct* st = ecs_get(w, comp, EcsStruct);
        if (!st) continue;
        const void* ptr = ecs_get_id(w, e, comp);
        if (!ptr) continue;

        const char* cname = ecs_get_name(w, comp);
        int mcount = ecs_vec_count(&st->members);
        const ecs_member_t* members = static_cast<const ecs_member_t*>(ecs_vec_first(&st->members));

        for (int m = 0; m < mcount; ++m) {
            bool is_opaque = false;
            ecs_primitive_kind_t k = member_kind(w, members[m].type, is_opaque);
            PropRow r;
            r.component = cname ? cname : "?";
            r.member = members[m].name ? members[m].name : "?";
            read_value(ptr, members[m].offset, k, is_opaque, r.kind, r.value);
            if (members[m].range.min != members[m].range.max) {
                r.has_range = true;
                r.range_min = members[m].range.min;
                r.range_max = members[m].range.max;
            }
            rows.push_back(std::move(r));
        }
    }
    return rows;
}

} // namespace ide::editor
