#include "table.hpp"

#include <cstdint>
#include <cstring>

namespace mat {
namespace {

bool section_fits(uint32_t offset, uint64_t bytes, std::size_t size, uint32_t align) {
    if (offset % align != 0) return false;
    return static_cast<uint64_t>(offset) + bytes <= size;
}

bool enum_ok(const ParamRow& p) {
    return p.type <= static_cast<uint8_t>(ParamType::Color) &&
           p.unit <= static_cast<uint8_t>(Unit::Degrees);
}

} // namespace

const char* load_reason(LoadResult r) {
    switch (r) {
    case LoadResult::Ok: return "ok";
    case LoadResult::TooShort: return "table shorter than its header";
    case LoadResult::BadMagic: return "magic is not LNMT";
    case LoadResult::BadVersion: return "unsupported table version";
    case LoadResult::BadLayout: return "a section does not fit the table";
    case LoadResult::BadString: return "string section is not terminated or an offset escapes it";
    case LoadResult::BadRange: return "a row points outside its section";
    case LoadResult::BadSlot: return "parameter slots overflow or overlap the instance block";
    case LoadResult::BadBase: return "material base is out of range or cyclic";
    case LoadResult::BadEnum: return "unknown parameter type, unit or blend";
    }
    return "unknown";
}

LoadResult Table::load(const void* base, std::size_t size) {
    // Обнуляются ВСЕ пять полей, а не только заголовок: отказавшая загрузка иначе оставляла
    // указатели предыдущей — `count()` отдавал ноль, но `row()`/`param()`/`name()` границ не
    // проверяют и читали бы уже снятый регион.
    header_ = nullptr;
    materials_ = nullptr;
    params_ = nullptr;
    textures_ = nullptr;
    strings_ = nullptr;
    // Выравнивание СЕКЦИЙ проверяется относительно `base`, поэтому сам `base` обязан быть
    // выровнен — иначе `reinterpret_cast` до `MaterialRow*` даёт UB, а на strict-align — SIGBUS.
    // Держалось это на том, что бандл приходит из mmap с `PAYLOAD_ALIGN`; контракт был нигде не
    // записан, и первый же `load(blob.data() + off, …)` с нечётным `off` вскрыл бы его молча.
    if (reinterpret_cast<std::uintptr_t>(base) % 8 != 0) return LoadResult::BadLayout;
    if (size < sizeof(TableHeader)) return LoadResult::TooShort;
    const auto* h = static_cast<const TableHeader*>(base);
    if (std::memcmp(h->magic, TABLE_MAGIC, 4) != 0) return LoadResult::BadMagic;
    if (h->version != TABLE_VERSION) return LoadResult::BadVersion;
    if (h->total_size != size) return LoadResult::BadLayout;

    const uint64_t mats = static_cast<uint64_t>(h->material_count) * sizeof(MaterialRow);
    const uint64_t prms = static_cast<uint64_t>(h->param_count) * sizeof(ParamRow);
    const uint64_t texs = static_cast<uint64_t>(h->texture_count) * sizeof(TextureRow);
    if (!section_fits(h->materials_offset, mats, size, 8)) return LoadResult::BadLayout;
    if (!section_fits(h->params_offset, prms, size, 4)) return LoadResult::BadLayout;
    if (!section_fits(h->textures_offset, texs, size, 8)) return LoadResult::BadLayout;
    if (h->strings_offset > size) return LoadResult::BadLayout;

    const char* str = static_cast<const char*>(base) + h->strings_offset;
    const std::size_t str_len = size - h->strings_offset;
    if (str_len == 0 || str[str_len - 1] != '\0') return LoadResult::BadString;

    const auto* materials = reinterpret_cast<const MaterialRow*>(static_cast<const uint8_t*>(base) +
                                                                h->materials_offset);
    const auto* params =
        reinterpret_cast<const ParamRow*>(static_cast<const uint8_t*>(base) + h->params_offset);
    const auto* textures =
        reinterpret_cast<const TextureRow*>(static_cast<const uint8_t*>(base) + h->textures_offset);

    for (uint32_t i = 0; i < h->param_count; ++i) {
        if (params[i].name_off >= str_len) return LoadResult::BadString;
        if (!enum_ok(params[i])) return LoadResult::BadEnum;
    }
    for (uint32_t i = 0; i < h->texture_count; ++i) {
        if (textures[i].name_off >= str_len) return LoadResult::BadString;
        if (textures[i].binding >= MAX_TEXTURES) return LoadResult::BadRange;
    }

    for (uint32_t i = 0; i < h->material_count; ++i) {
        const MaterialRow& m = materials[i];
        if (m.name_off >= str_len || m.shader_off >= str_len) return LoadResult::BadString;
        if (m.blend > static_cast<uint8_t>(Blend::Additive)) return LoadResult::BadEnum;
        if (static_cast<uint64_t>(m.param_first) + m.param_count > h->param_count)
            return LoadResult::BadRange;
        if (static_cast<uint64_t>(m.texture_first) + m.texture_count > h->texture_count)
            return LoadResult::BadRange;

        uint32_t used = 0;
        for (uint32_t p = 0; p < m.param_count; ++p) {
            const ParamRow& row = params[m.param_first + p];
            const uint32_t floats = param_floats(static_cast<ParamType>(row.type));
            if (static_cast<uint32_t>(row.slot) + floats > PARAM_BLOCK_FLOATS)
                return LoadResult::BadSlot;
            const uint32_t mask = ((1u << floats) - 1u) << row.slot;
            if ((used & mask) != 0) return LoadResult::BadSlot;
            used |= mask;
        }

        // Цепочка баз обходится счётчиком, а не пометками: предел тот же `MAX_BASE_DEPTH`, что
        // и у обходов ниже, поэтому цикл и слишком глубокая цепь отбиваются здесь, а `resolve`
        // никогда не упирается в свой предел на принятой таблице.
        uint32_t hops = 0;
        uint16_t at = m.base;
        while (at != NO_BASE) {
            if (at >= h->material_count) return LoadResult::BadBase;
            if (++hops >= MAX_BASE_DEPTH) return LoadResult::BadBase;
            at = materials[at].base;
        }
    }

    header_ = h;
    materials_ = materials;
    params_ = params;
    textures_ = textures;
    strings_ = str;
    return LoadResult::Ok;
}

uint32_t Table::find(const char* material_name) const {
    for (uint32_t i = 0; i < count(); ++i)
        if (std::strcmp(strings_ + materials_[i].name_off, material_name) == 0) return i;
    return count();
}

void Table::resolve(uint32_t i, float out[PARAM_BLOCK_FLOATS]) const {
    for (uint32_t f = 0; f < PARAM_BLOCK_FLOATS; ++f) out[f] = 0.0f;
    if (i >= count()) return;

    // База пишется ПЕРВОЙ, потомок поверх неё: переопределение — это последняя запись в слот.
    uint16_t chain[MAX_BASE_DEPTH];
    uint32_t depth = 0;
    for (uint32_t at = i; depth < MAX_BASE_DEPTH;) {
        chain[depth++] = static_cast<uint16_t>(at);
        const uint16_t next = materials_[at].base;
        if (next == NO_BASE) break;
        at = next;
    }
    for (uint32_t d = depth; d-- > 0;) {
        const MaterialRow& m = materials_[chain[d]];
        for (uint32_t p = 0; p < m.param_count; ++p) {
            const ParamRow& row = params_[m.param_first + p];
            const uint32_t floats = param_floats(static_cast<ParamType>(row.type));
            for (uint32_t f = 0; f < floats; ++f) out[row.slot + f] = row.value[f];
        }
    }
}

int32_t Table::slot_of(uint32_t i, const char* param_name) const {
    if (i >= count()) return -1;
    for (uint32_t at = i, depth = 0; depth < MAX_BASE_DEPTH; ++depth) {
        const MaterialRow& m = materials_[at];
        for (uint32_t p = 0; p < m.param_count; ++p) {
            const ParamRow& row = params_[m.param_first + p];
            if (std::strcmp(strings_ + row.name_off, param_name) == 0)
                return static_cast<int32_t>(row.slot);
        }
        if (m.base == NO_BASE) break;
        at = m.base;
    }
    return -1;
}

} // namespace mat
