#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../asset/hash.hpp"
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

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

const char* const SOURCE =
    "# library of the sample effects (spec #18)\n"
    "material | flash | sprite_flash | alpha\n"
    "param | tint     | color  | raw      | 1,1,1,1\n"
    "param | strength | scalar | fraction | 0\n"
    "\n"
    "material | outline | sprite_outline | alpha\n"
    "param | color     | color  | raw    | 0,0,0,1\n"
    "param | thickness | scalar | pixels | 1\n"
    "\n"
    "material | dissolve | sprite_dissolve | alpha\n"
    "param | threshold | scalar | fraction | 0\n"
    "param | edge      | scalar | fraction | 0.08\n"
    "tex   | noise     | noise_rgba | 1\n"
    "\n"
    "instance | hit_flash | flash\n"
    "set | strength | 1\n"
    "set | tint     | 1,0.25,0.25,1\n";

void test_shape() {
    std::vector<uint8_t> bytes;
    mat::BakeError err;
    check(mat::bake_materials(SOURCE, bytes, err), "the library source bakes");

    mat::Table t;
    const mat::LoadResult r = t.load(bytes.data(), bytes.size());
    check(r == mat::LoadResult::Ok, mat::load_reason(r));
    check(t.count() == 4, "four materials, the instance among them");
    check(t.find("hit_flash") == 3, "the instance is found by name");
    check(t.find("nope") == t.count(), "a name that is not there returns count(), not zero");

    const mat::MaterialRow& flash = t.row(0);
    const mat::MaterialRow& inst = t.row(3);
    check(inst.base == 0, "the instance points at its base");
    check(inst.shader_guid == flash.shader_guid, "the instance inherits the shader");
    check(flash.shader_guid == asset::fnv1a("sprite_flash", 12),
          "shader guid is fnv1a of the logical name, as in assetc");
    // Имя шейдера строкой, а не только хешем: точку входа в модуле нечем назвать, кроме неё, и у
    // инстанса она обязана быть УНАСЛЕДОВАННОЙ, а не пустой.
    check(std::strcmp(t.shader(0), "sprite_flash") == 0, "shader name reaches the reader");
    check(std::strcmp(t.shader(3), t.shader(0)) == 0, "the instance inherits the shader name");

    const mat::MaterialRow& dis = t.row(2);
    check(dis.texture_count == 1, "dissolve declares one texture slot");
    const mat::TextureRow& noise = t.texture(dis.texture_first);
    check(noise.binding == 1 && noise.guid == asset::fnv1a("noise_rgba", 10),
          "texture slot carries binding and the asset guid");
}

void test_slots_and_inheritance() {
    std::vector<uint8_t> bytes;
    mat::BakeError err;
    check(mat::bake_materials(SOURCE, bytes, err), "source bakes");
    mat::Table t;
    check(t.load(bytes.data(), bytes.size()) == mat::LoadResult::Ok, "table loads");

    float base[mat::PARAM_BLOCK_FLOATS];
    t.resolve(0, base);
    check(base[0] == 1.0f && base[1] == 1.0f && base[2] == 1.0f && base[3] == 1.0f,
          "flash tint occupies the first four floats");
    check(base[4] == 0.0f, "flash strength sits after the colour, not on top of it");

    float derived[mat::PARAM_BLOCK_FLOATS];
    t.resolve(3, derived);
    check(derived[4] == 1.0f, "the instance overrides the inherited scalar");
    check(derived[0] == 1.0f && near(derived[1], 0.25f) && near(derived[2], 0.25f),
          "the instance overrides the inherited colour");
    check(derived[5] == 0.0f && derived[6] == 0.0f && derived[7] == 0.0f,
          "slots nobody declared stay zero");

    // Переопределение обязано сесть В ТОТ ЖЕ слот, что и база: разъехавшийся слот дал бы шейдеру
    // значение по другому смещению, и на глаз это выглядит как «параметр не применился».
    const mat::MaterialRow& inst = t.row(3);
    const mat::MaterialRow& flash = t.row(0);
    check(t.param(inst.param_first).slot == t.param(flash.param_first + 1).slot,
          "the override keeps the slot of the parameter it overrides");
}

void test_units() {
    const char* const src =
        "material | u | sprite_u | opaque\n"
        "param | over  | scalar | fraction | 1.5\n"
        "param | under | scalar | fraction | -0.25\n"
        "param | turn  | scalar | degrees  | 180\n"
        "param | thick | scalar | pixels   | 2.5\n";
    std::vector<uint8_t> bytes;
    mat::BakeError err;
    check(mat::bake_materials(src, bytes, err), "units source bakes");
    mat::Table t;
    check(t.load(bytes.data(), bytes.size()) == mat::LoadResult::Ok, "units table loads");
    float v[mat::PARAM_BLOCK_FLOATS];
    t.resolve(0, v);
    check(v[0] == 1.0f, "a fraction above one is clamped");
    check(v[1] == 0.0f, "a negative fraction is clamped");
    check(near(v[2], 3.14159265f), "degrees are stored as radians");
    check(near(v[3], 2.5f), "pixels are stored as authored");
}

void test_determinism() {
    std::vector<uint8_t> a, b;
    mat::BakeError err;
    check(mat::bake_materials(SOURCE, a, err), "first bake");
    check(mat::bake_materials(SOURCE, b, err), "second bake");
    check(a == b, "the same source bakes to the same bytes");
    check(!a.empty() && a.size() == reinterpret_cast<const mat::TableHeader*>(a.data())->total_size,
          "the header states the size the reader is handed");
}

} // namespace

int main() {
    std::printf("material bake seam\n");
    test_shape();
    test_slots_and_inheritance();
    test_units();
    test_determinism();
    std::printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
