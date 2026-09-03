#include "material_slots_test.hpp"

#include <cstdio>

#include "../asset/hash.hpp"

namespace matlib {
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

} // namespace

int check_texture_slots(const mat::Table& t) {
    fails = 0;
    // Текстура эффекта живёт в СВОЁМ слоте, а не в слоте альбедо: шейдер берёт шум из `aux`
    // (@binding(3) = слот 1 материала), и слот 0 обязан остаться за спрайтом.
    const uint32_t d = t.find("dissolve");
    if (d != t.count()) {
        const int32_t noise = t.texture_of(d, "noise");
        check(noise >= 0, "dissolve declares its noise slot");
        if (noise >= 0) {
            const mat::TextureRow& tex = t.texture(static_cast<uint32_t>(noise));
            check(tex.binding == 1, "dissolve noise sits in the aux slot, not albedo");
            check(tex.guid == asset::fnv1a("noise_rgba", 10), "dissolve names the noise asset");
        }
        // Слот 0 остаётся за спрайтом, и утверждается это перебором, а не числом текстур: с шагом C
        // у материала их две, и `texture_count == 1` проверяло бы уже не то, ради чего написано.
        for (uint32_t k = 0; k < t.row(d).texture_count; ++k)
            check(t.texture(t.row(d).texture_first + k).binding != 0,
                  "no slot of dissolve takes the albedo binding");
    }

    // Слот нормали — шов между `library.mat` и ПРОХОДОМ ОСВЕЩЕНИЯ, а не шейдером библиотеки: его
    // номер и guid ассета читает другой потребитель, и разъехаться им нечем, кроме этих строк.
    const uint32_t fg = t.find("flash_gold");
    const int32_t nrm = fg == t.count() ? -1 : t.texture_of(fg, "normal");
    check(nrm >= 0 && t.texture(static_cast<uint32_t>(nrm)).binding == 2 &&
              t.texture(static_cast<uint32_t>(nrm)).guid == asset::fnv1a("sprite_normal", 13),
          "flash_gold inherits the sprite normal map in slot 2");
    check(t.texture_of(t.find("dissolve"), "normal") < 0, "dissolve declares no normal slot");

    // Слот перекрытия (шаг C гейта 7) — тот же шов и тот же класс дефекта: номер и guid читает
    // проход теней. Набор материалов у него НАМЕРЕННО другой, и это утверждается парой — `outline`
    // отбрасывает и светится, `flash` только светится, `dissolve` только отбрасывает.
    const uint32_t ol = t.find("outline");
    const int32_t occ = ol == t.count() ? -1 : t.texture_of(ol, "occlusion");
    check(occ >= 0 && t.texture(static_cast<uint32_t>(occ)).binding == 3 &&
              t.texture(static_cast<uint32_t>(occ)).guid == asset::fnv1a("sprite_occluder", 15),
          "outline declares the disc occluder in slot 3");
    check(t.texture_of(t.find("dissolve_ash"), "occlusion") >= 0,
          "dissolve_ash inherits the grate occluder of its base");
    check(t.texture_of(t.find("flash"), "occlusion") < 0, "flash casts nothing");
    return fails;
}

} // namespace matlib
