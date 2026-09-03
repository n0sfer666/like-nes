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

// Невыровненный `base` обязан быть отбит кодом, а не UB: выравнивание секций проверяется
// ОТНОСИТЕЛЬНО базы, поэтому нечётная база даёт невыровненный `reinterpret_cast` до `MaterialRow*`
// — на strict-align это SIGBUS, а не «немного медленнее».
void test_unaligned_base() {
    std::vector<uint8_t> b = baked();
    std::vector<uint8_t> shifted(b.size() + 1);
    std::memcpy(shifted.data() + 1, b.data(), b.size());
    mat::Table t;
    check(t.load(shifted.data() + 1, b.size()) == mat::LoadResult::BadLayout,
          "an unaligned base is refused, not read");
}

// Предел глубины наследования проверяется НА ОБОИХ КОНЦАХ: голден из середины диапазона слеп к
// дефектам на границе, а здесь граница и есть предмет. Цепь ровно в `MAX_BASE_DEPTH` материалов
// обязана грузиться и разворачиваться, длиннее — отбиваться `BadBase`, а не терять корни молча.
void test_base_depth_limit() {
    for (uint32_t extra = 0; extra <= 1; ++extra) {
        const uint32_t n = mat::MAX_BASE_DEPTH + extra;
        std::string src = "material | m0 | sprite_flash | alpha\n"
                          "param | strength | scalar | raw | 0.5\n";
        for (uint32_t i = 1; i < n; ++i)
            src += "instance | m" + std::to_string(i) + " | m" + std::to_string(i - 1) + "\n";
        std::vector<uint8_t> bytes;
        mat::BakeError err;
        if (!mat::bake_materials(src, bytes, err)) { check(false, "depth fixture bakes"); continue; }
        mat::Table t;
        const mat::LoadResult r = t.load(bytes.data(), bytes.size());
        if (extra == 0) {
            check(r == mat::LoadResult::Ok, "a chain exactly at the depth limit still loads");
            float p[mat::PARAM_BLOCK_FLOATS];
            t.resolve(n - 1, p);
            check(p[0] == 0.5f, "the deepest child still inherits the root's parameter");
        } else {
            check(r == mat::LoadResult::BadBase, "a chain past the depth limit is refused");
        }
    }
}

// Текстурный слот адресуется ИМЕНЕМ и наследуется от базы так же, как параметр: инстанс не
// перечисляет чужие текстуры заново. Без этого утверждения `texture_of` неотличим от поиска
// только по собственным строкам материала — а инстансы в библиотеке как раз своих не имеют.
void test_texture_of() {
    std::vector<uint8_t> b = baked();
    mat::Table t;
    check(t.load(b.data(), b.size()) == mat::LoadResult::Ok, "the fixture opens");
    const int32_t own = t.texture_of(0, "noise");
    const int32_t inherited = t.texture_of(1, "noise");
    check(own >= 0 && own == inherited, "the instance resolves the texture slot of its base");
    check(t.texture_of(0, "normal") < 0, "a slot the material never declared has no index");
    check(t.texture_of(t.count(), "noise") < 0, "a material outside the table has no slot");
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
    test_texture_of();
    test_unaligned_base();
    test_base_depth_limit();
    test_reasons_are_distinct();
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
