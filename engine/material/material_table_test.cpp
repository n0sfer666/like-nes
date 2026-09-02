#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bake.hpp"
#include "table.hpp"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

const char* const SOURCE =
    "material | flash | sprite_flash | alpha\n"
    "param | tint     | color  | raw      | 1,1,1,1\n"
    "param | strength | scalar | fraction | 0\n"
    "tex   | noise    | noise_rgba | 1\n"
    "instance | hit | flash\n"
    "set | strength | 1\n";

std::vector<uint8_t> baked() {
    std::vector<uint8_t> bytes;
    mat::BakeError err;
    if (!mat::bake_materials(SOURCE, bytes, err)) std::printf("  FAIL fixture does not bake\n");
    return bytes;
}

mat::TableHeader* head(std::vector<uint8_t>& b) {
    return reinterpret_cast<mat::TableHeader*>(b.data());
}
mat::MaterialRow* materials(std::vector<uint8_t>& b) {
    return reinterpret_cast<mat::MaterialRow*>(b.data() + head(b)->materials_offset);
}
mat::ParamRow* params(std::vector<uint8_t>& b) {
    return reinterpret_cast<mat::ParamRow*>(b.data() + head(b)->params_offset);
}
mat::TextureRow* textures(std::vector<uint8_t>& b) {
    return reinterpret_cast<mat::TextureRow*>(b.data() + head(b)->textures_offset);
}

template <typename F>
void refuses(const char* what, mat::LoadResult want, F mutate) {
    std::vector<uint8_t> b = baked();
    mutate(b);
    mat::Table t;
    const mat::LoadResult got = t.load(b.data(), b.size());
    check(got == want, what);
    if (got != want) std::printf("    got %s\n", mat::load_reason(got));
    check(t.count() == 0, "a refused table leaves no rows behind");
}

// Позитивный контроль: та же фикстура БЕЗ правки обязана грузиться. Без него набор ниже зелен и на
// читателе, который отвергает всё подряд, — то есть проверяет не таблицу, а сам себя.
void test_intact() {
    std::vector<uint8_t> b = baked();
    mat::Table t;
    const mat::LoadResult r = t.load(b.data(), b.size());
    check(r == mat::LoadResult::Ok, mat::load_reason(r));
    check(t.count() == 2, "the fixture the corrupted copies are made of");
}

void test_corruption() {
    refuses("a table shorter than its header", mat::LoadResult::TooShort,
            [](std::vector<uint8_t>& b) { b.resize(8); });
    refuses("someone else's bytes under our magic", mat::LoadResult::BadMagic,
            [](std::vector<uint8_t>& b) { head(b)->magic[0] = 'X'; });
    refuses("a version this reader does not know", mat::LoadResult::BadVersion,
            [](std::vector<uint8_t>& b) { head(b)->version = mat::TABLE_VERSION + 1; });
    refuses("a size that disagrees with the bytes handed in", mat::LoadResult::BadLayout,
            [](std::vector<uint8_t>& b) { head(b)->total_size += 1; });
    refuses("rows starting off their alignment", mat::LoadResult::BadLayout,
            [](std::vector<uint8_t>& b) { head(b)->materials_offset += 4; });
    refuses("a section running past the end", mat::LoadResult::BadLayout,
            [](std::vector<uint8_t>& b) { head(b)->material_count = 4096; });
    refuses("a string section with no terminator", mat::LoadResult::BadString,
            [](std::vector<uint8_t>& b) { b.back() = 'x'; });
    refuses("a name pointing outside the strings", mat::LoadResult::BadString,
            [](std::vector<uint8_t>& b) { materials(b)[0].name_off = head(b)->total_size; });
    refuses("a material owning parameters that are not there", mat::LoadResult::BadRange,
            [](std::vector<uint8_t>& b) { materials(b)[0].param_count = 99; });
    refuses("a colour hanging off the end of the instance block", mat::LoadResult::BadSlot,
            [](std::vector<uint8_t>& b) { params(b)[0].slot = mat::PARAM_BLOCK_FLOATS - 1; });
    refuses("two parameters sharing floats", mat::LoadResult::BadSlot,
            [](std::vector<uint8_t>& b) { params(b)[1].slot = 0; });
    refuses("a base index that is not a material", mat::LoadResult::BadBase,
            [](std::vector<uint8_t>& b) { materials(b)[1].base = 99; });
    refuses("a material inheriting from itself", mat::LoadResult::BadBase,
            [](std::vector<uint8_t>& b) { materials(b)[1].base = 1; });
    refuses("a blend mode outside the enum", mat::LoadResult::BadEnum,
            [](std::vector<uint8_t>& b) { materials(b)[0].blend = 9; });
    refuses("a parameter type outside the enum", mat::LoadResult::BadEnum,
            [](std::vector<uint8_t>& b) { params(b)[0].type = 9; });
    refuses("a texture binding past the slot count", mat::LoadResult::BadRange,
            [](std::vector<uint8_t>& b) { textures(b)[0].binding = 9; });
}

// Смещение по имени обязано разворачивать базу так же, как `resolve`: `hit` объявляет только
// `strength`, а `tint` наследует. Читатель, ищущий имя лишь в собственных строках материала,
// отдал бы на `tint` минус единицу, и потребитель молча перестал бы красить.
void test_slot_of() {
    std::vector<uint8_t> b = baked();
    mat::Table t;
    if (t.load(b.data(), b.size()) != mat::LoadResult::Ok) { check(false, "fixture loads"); return; }
    const uint32_t base = t.find("flash"), inst = t.find("hit");
    check(t.slot_of(base, "tint") == 0, "the base finds its own colour");
    check(t.slot_of(base, "strength") == 4, "the base finds its own scalar");
    check(t.slot_of(inst, "strength") == 4, "the instance finds the slot it overrides");
    check(t.slot_of(inst, "tint") == 0, "the instance inherits the slot it never names");
    check(t.slot_of(inst, "nope") == -1, "a name no one declares is not a slot");
    check(t.slot_of(t.count(), "tint") == -1, "a material that is not there has no slots");
}

// Каждая причина отказа обязана иметь СВОИ слова: две ветки с одним текстом читаются в логе
// одинаково, и различить их можно только по коду, которого в логе нет.
void test_reasons_are_distinct() {
    const mat::LoadResult all[] = {
        mat::LoadResult::Ok,       mat::LoadResult::TooShort, mat::LoadResult::BadMagic,
        mat::LoadResult::BadVersion, mat::LoadResult::BadLayout, mat::LoadResult::BadString,
        mat::LoadResult::BadRange, mat::LoadResult::BadSlot,  mat::LoadResult::BadBase,
        mat::LoadResult::BadEnum};
    for (std::size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
        for (std::size_t j = i + 1; j < sizeof(all) / sizeof(all[0]); ++j)
            check(std::strcmp(mat::load_reason(all[i]), mat::load_reason(all[j])) != 0,
                  "two refusals share their wording");
}

} // namespace

int main() {
    std::printf("material table reader\n");
    test_intact();
    test_corruption();
    test_slot_of();
    test_reasons_are_distinct();
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
