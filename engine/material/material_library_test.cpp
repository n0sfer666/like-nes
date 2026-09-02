#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../asset/hash.hpp"
#include "../platform/platform_args.hpp"
#include "bake.hpp"
#include "param.hpp"
#include "table.hpp"

// Шов «библиотека эффектов и её шейдер говорят про одни и те же смещения».
//
// Слоты параметров не пишутся руками: пекарь раздаёт их в порядке объявления в `library.mat`, а
// `sprite_effects.wgsl` читает их номерами (`p0`, `p1.x`). Между этими двумя файлами нет ни одной
// проверки со стороны компилятора: перестановка двух строк в `.mat` не ломает ни сборку, ни бейк —
// эффект просто начинает брать толщину обводки из альфы цвета. Кадр при этом рисуется, поэтому и
// голден #18 гейт 1 отбил бы такое лишь на глаз владельца и лишь на той машине, где он смотрел.
//
// Здесь мирроринг стоит утверждением: имя параметра → номер слота, имя материала → имя фрагментной
// точки входа, которая ОБЯЗАНА найтись в тексте шейдера. Плюс позитивный контроль: сама библиотека
// обязана печься и открываться, иначе «расхождений нет» значит «ничего не прочитано».
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* DEFAULT_DIR = "engine/material/library";

// Ожидаемая раскладка. Это не пересказ `library.mat`, а контракт, на который смотрит WGSL: числа
// ниже стоят в шейдере как `p0`/`p1.x`/`p1.y`.
struct SlotPin {
    const char* material;
    const char* param;
    uint8_t slot;
    mat::ParamType type;
    mat::Unit unit;
};

const SlotPin PINS[] = {
    {"flash", "tint", 0, mat::ParamType::Color, mat::Unit::Raw},
    {"flash", "strength", 4, mat::ParamType::Scalar, mat::Unit::Fraction},
    {"outline", "color", 0, mat::ParamType::Color, mat::Unit::Raw},
    {"outline", "thickness", 4, mat::ParamType::Scalar, mat::Unit::Pixels},
    {"dissolve", "edge_color", 0, mat::ParamType::Color, mat::Unit::Raw},
    {"dissolve", "threshold", 4, mat::ParamType::Scalar, mat::Unit::Fraction},
    {"dissolve", "edge", 5, mat::ParamType::Scalar, mat::Unit::Fraction},
};

struct ShaderPin {
    const char* material;
    const char* shader;
    const char* entry;
};

const ShaderPin SHADERS[] = {
    {"flash", "sprite_flash", "fn fs_flash("},
    {"outline", "sprite_outline", "fn fs_outline("},
    {"dissolve", "sprite_dissolve", "fn fs_dissolve("},
};

// Разрешённый блок параметров целиком, а не «сколько чисел разошлось»: инстанс, потерявший
// наследование, отличался бы от базы в тех же двух числах, что и правильный, и счётчик расхождений
// молчал бы. Инстанс обязан ещё и делить пайплайн с базой — решение 2 спеки #18: инстанс дёшев.
struct ResolvePin {
    const char* material;
    const char* base;
    float want[mat::PARAM_BLOCK_FLOATS];
};

const ResolvePin RESOLVED[] = {
    {"flash", nullptr, {1, 1, 1, 1, 0, 0, 0, 0}},
    {"flash_red", "flash", {1, 0.15f, 0.1f, 1, 0, 0, 0, 0}},
    {"flash_gold", "flash", {1, 0.85f, 0.25f, 1, 0.5f, 0, 0, 0}},
    {"outline", nullptr, {0, 0, 0, 1, 1, 0, 0, 0}},
    {"outline_danger", "outline", {0.95f, 0.1f, 0.1f, 1, 2, 0, 0, 0}},
    {"dissolve", nullptr, {1, 0.55f, 0.1f, 1, 0, 0.08f, 0, 0}},
    {"dissolve_ash", "dissolve", {0.6f, 0.6f, 0.62f, 1, 0, 0.16f, 0, 0}},
};

bool read_file(const std::string& path, std::string& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[4096];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

const mat::ParamRow* find_param(const mat::Table& t, uint32_t m, const char* name) {
    for (uint32_t hops = 0; m < t.count() && hops <= t.count(); ++hops) {
        const mat::MaterialRow& r = t.row(m);
        for (uint16_t i = 0; i < r.param_count; ++i) {
            const mat::ParamRow& p = t.param(r.param_first + i);
            if (std::strcmp(t.name(p.name_off), name) == 0) return &p;
        }
        if (r.base == mat::NO_BASE) break;
        m = r.base;
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("effect library mirrors its shader\n");
    const std::string dir = argc > 1 ? argv[1] : DEFAULT_DIR;

    std::string src;
    std::string wgsl;
    if (!read_file(dir + "/library.mat", src) ||
        !read_file(dir + "/sprite_effects.wgsl", wgsl)) {
        std::printf("  FAIL: library not readable under %s\n", dir.c_str());
        std::printf("material-library: FAIL\n");
        return 1;
    }

    std::vector<uint8_t> bytes;
    mat::BakeError err;
    if (!mat::bake_materials(src, bytes, err)) {
        std::printf("  FAIL: library.mat:%d: %s\n", err.line, err.message.c_str());
        std::printf("material-library: FAIL\n");
        return 1;
    }

    mat::Table t;
    const mat::LoadResult lr = t.load(bytes.data(), bytes.size());
    if (lr != mat::LoadResult::Ok) {
        std::printf("  FAIL: baked library does not open: %s\n", mat::load_reason(lr));
        std::printf("material-library: FAIL\n");
        return 1;
    }
    std::printf("  library: %u material(s), %zu bytes\n", t.count(), bytes.size());
    check(t.count() >= 7, "library carries the three effects and their instances");

    for (const SlotPin& p : PINS) {
        const uint32_t m = t.find(p.material);
        if (m == t.count()) {
            std::printf("  FAIL: material '%s' missing from the library\n", p.material);
            ++fails;
            continue;
        }
        const mat::ParamRow* row = find_param(t, m, p.param);
        if (!row) {
            std::printf("  FAIL: %s: no parameter '%s'\n", p.material, p.param);
            ++fails;
            continue;
        }
        if (row->slot != p.slot)
            std::printf("  FAIL: %s.%s sits at slot %u, shader reads slot %u\n", p.material,
                        p.param, row->slot, p.slot);
        check(row->slot == p.slot, "parameter slot agrees with the shader");
        check(row->type == static_cast<uint8_t>(p.type), "parameter type agrees with the shader");
        check(row->unit == static_cast<uint8_t>(p.unit), "parameter unit agrees with the shader");
    }

    for (const ShaderPin& s : SHADERS) {
        const uint32_t m = t.find(s.material);
        if (m == t.count()) {
            ++fails;
            std::printf("  FAIL: material '%s' missing from the library\n", s.material);
            continue;
        }
        const uint64_t want = asset::fnv1a(s.shader, std::strlen(s.shader));
        if (t.row(m).shader_guid != want)
            std::printf("  FAIL: %s names a shader other than '%s'\n", s.material, s.shader);
        check(t.row(m).shader_guid == want, "material names the library shader");
        if (wgsl.find(s.entry) == std::string::npos)
            std::printf("  FAIL: sprite_effects.wgsl has no entry point '%s'\n", s.entry);
        check(wgsl.find(s.entry) != std::string::npos, "shader carries the entry point");
    }

    for (const ResolvePin& r : RESOLVED) {
        const uint32_t i = t.find(r.material);
        if (i == t.count()) {
            ++fails;
            std::printf("  FAIL: material '%s' missing from the library\n", r.material);
            continue;
        }
        if (r.base) {
            const uint32_t b = t.find(r.base);
            check(b != t.count() && t.row(i).base == b, "instance points at its base");
            if (b != t.count()) {
                check(t.row(i).shader_guid == t.row(b).shader_guid,
                      "instance shares the base pipeline");
                check(t.row(i).blend == t.row(b).blend, "instance shares the base blending");
            }
        } else {
            check(t.row(i).base == mat::NO_BASE, "root material has no base");
        }

        float got[mat::PARAM_BLOCK_FLOATS];
        t.resolve(i, got);
        for (uint32_t k = 0; k < mat::PARAM_BLOCK_FLOATS; ++k) {
            if (got[k] == r.want[k]) continue;
            ++fails;
            std::printf("  FAIL: %s: float %u is %g, expected %g\n", r.material, k,
                        static_cast<double>(got[k]), static_cast<double>(r.want[k]));
        }
    }

    // Текстура эффекта живёт в СВОЁМ слоте, а не в слоте альбедо: шейдер берёт шум из `aux`
    // (@binding(3) = слот 1 материала), и слот 0 обязан остаться за спрайтом.
    const uint32_t d = t.find("dissolve");
    if (d != t.count()) {
        check(t.row(d).texture_count == 1, "dissolve declares one texture");
        if (t.row(d).texture_count == 1) {
            const mat::TextureRow& tex = t.texture(t.row(d).texture_first);
            check(tex.binding == 1, "dissolve noise sits in the aux slot, not albedo");
            check(tex.guid == asset::fnv1a("noise_rgba", 10), "dissolve names the noise asset");
        }
    }

    std::printf("material-library: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
