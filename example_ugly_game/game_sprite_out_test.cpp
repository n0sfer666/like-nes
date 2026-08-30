#include <cmath>
#include <cstdio>

#include "art.hpp"
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "graphics/sprite.hpp"
#include "instance_stage.hpp"
#include "platform_args.hpp"
#include "sprite_out.hpp"

// Шов «спрайт фреймворка → инстанс шутера» (спека #17, вертикаль 3, шаг B3). Гейт стоит отдельно от
// частиц потому, что предмет у него другой: частицы отвечают, ЧТО нарисовать, а шов — в каких
// единицах. Три перевода в нём, и каждый уже был бы тихой ошибкой: полуразмер в полный, номер
// региона в UV и восемь бит канала в яркость HDR-цели.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) { std::printf("  FAIL: %s\n", what); ++fails; }
}

using framework::Vec2;
using framework::graphics::Sprite;
using framework::graphics::SpriteList;

constexpr uint32_t CAP = 16;
Sprite sprites[CAP];
uint64_t keys[CAP];
game::Instance insts[CAP];

bool near(float a, float b) { return std::fabs(a - b) < 1.0f / 4096.0f; }

Sprite made(uint16_t region, uint16_t material, uint32_t rgba) {
    Sprite s;
    s.center = {fix32::from_int(10), fix32::from_int(-3)};
    s.half = {fix32::from_int(4), fix32::from_int(6)};
    s.rgba = rgba;
    s.region = region;
    s.material = material;
    return s;
}

// Геометрия и нарезка: полуразмер удваивается, UV берутся ИЗ АТЛАСА по номеру региона, а не из
// самого спрайта — номера в вершинном буфере нет вовсе.
void test_geometry(const game::Atlas& atlas) {
    SpriteList list(sprites, keys, CAP);
    list.push(made(game::RID_Star, game::MAT_Flat, 0xffffffffu));
    check(game::sprites_to_instances(list, atlas, insts, CAP) == 1, "one sprite is one instance");
    check(near(insts[0].x, 10) && near(insts[0].y, -3), "the centre passes through unchanged");
    check(near(insts[0].w, 8) && near(insts[0].h, 12),
          "and the half size becomes the whole one, both sides");
    const game::Region* r = game::region_at(atlas, game::RID_Star);
    check(insts[0].u0 == r->u0 && insts[0].v0 == r->v0 && insts[0].u1 == r->u1 &&
              insts[0].v1 == r->v1,
          "the region number resolves to the cut the atlas holds under it");
}

// Экспозиция материала: яркость выше единицы восемью битами канала не выражается, и именно ею
// bloom берётся за частицу. Прозрачность при этом остаётся долей — умножить её значило бы сделать
// светящуюся частицу заодно непрозрачной.
void test_exposure(const game::Atlas& atlas) {
    SpriteList list(sprites, keys, CAP);
    list.push(made(game::RID_Star, game::MAT_Glow, 0xff9e3880u));
    list.push(made(game::RID_Star, game::MAT_Flat, 0xff9e3880u));
    list.push(made(game::RID_Star, game::MAT_Count, 0xff9e3880u));
    check(game::sprites_to_instances(list, atlas, insts, CAP) == 3, "all three are laid out");
    const float a = 128.0f / 255.0f;
    check(near(insts[0].r, 1.9f) && near(insts[0].g, 158.0f / 255.0f * 1.9f) &&
              near(insts[0].b, 56.0f / 255.0f * 1.9f),
          "the glow material lifts every colour channel by its exposure");
    check(near(insts[0].a, a), "and leaves the alpha alone");
    check(near(insts[1].r, 1.0f) && near(insts[1].g, 158.0f / 255.0f) && near(insts[1].a, a),
          "the flat material changes nothing at all");
    check(near(insts[2].r, 1.0f), "a material past the table is flat, not a wild multiplier");
}

// Регион вне нарезки не рисуется, а не берёт угол страницы: номер приходит СНАРУЖИ, из таблицы
// описаний, и молчаливый мусор в кадре был бы виден только глазами.
void test_refusals(const game::Atlas& atlas) {
    SpriteList list(sprites, keys, CAP);
    list.push(made(game::RID_None, game::MAT_Flat, 0xffffffffu));
    list.push(made(game::RID_Count, game::MAT_Flat, 0xffffffffu));
    list.push(made(game::RID_Ship, game::MAT_Flat, 0xffffffffu));
    check(game::sprites_to_instances(list, atlas, insts, CAP) == 1,
          "neither the zero region nor one past the cut reaches the vertex buffer");
    const game::Region* r = game::region_at(atlas, game::RID_Ship);
    check(insts[0].u0 == r->u0, "and the one that survives is the one that was drawable");
    check(game::sprites_to_instances(list, atlas, nullptr, CAP) == 0,
          "without a buffer nothing is written and nothing is claimed");
}

// Потолок буфера: лишнее ОТБРАСЫВАЕТСЯ, а не пишется за границу. Буфером владеет вызывающий (гейт
// 8), и запись мимо него — единственный отказ, которого не видно вообще ниоткуда.
void test_overflow(const game::Atlas& atlas) {
    SpriteList list(sprites, keys, CAP);
    for (uint32_t i = 0; i < 8; ++i) list.push(made(game::RID_Star, game::MAT_Flat, 0xffffffffu));
    check(game::sprites_to_instances(list, atlas, insts, 3) == 3,
          "a buffer of three takes three of the eight");
    check(game::sprites_to_instances(list, atlas, insts, 0) == 0, "and a buffer of none takes none");
}

// Направление спрайта — единичный вектор, поворот квада — угол. Тождественное направление обязано
// дать РОВНЫЙ ноль: инстанс с почти-нулём поворачивал бы неподвижный спрайт на доли градуса, и
// заметно это стало бы только на пиксельной сетке.
void test_rotation(const game::Atlas& atlas) {
    SpriteList list(sprites, keys, CAP);
    Sprite up = made(game::RID_Star, game::MAT_Flat, 0xffffffffu);
    up.dir = {fix32{}, fix32::from_int(1)};
    list.push(made(game::RID_Star, game::MAT_Flat, 0xffffffffu));
    list.push(up);
    check(game::sprites_to_instances(list, atlas, insts, CAP) == 2, "both are laid out");
    check(insts[0].rot == 0.0f, "the identity direction is exactly no rotation");
    check(near(insts[1].rot, 1.5707963f), "and a quarter turn down is a quarter turn of the quad");
}

// Последний шов перед видеокартой: накопитель кадра. Гейт 8 спеки #17 требовал загрузки в вершинный
// буфер и утверждения на ней, а `SpriteBatch` для утверждения не годится — он открывает устройство,
// текстуру и конвейер, которых на раннере нет. Поэтому накопление отделено от загрузки
// (`InstanceStage`), и здесь проверяется ровно накопление: ёмкость не растёт, лишнее ОТКАЗЫВАЕТ и
// считается, длина в байтах следует за числом инстансов, а установившийся кадр не ходит в кучу.
void test_stage() {
    constexpr uint32_t N = 64;
    constexpr uint32_t GUARD = 4;
    // Буфер ДЛИННЕЕ объявленной ёмкости, а хвост остаётся сторожевым. Без него граница, сдвинутая
    // на единицу (`>` вместо `>=`), роняет прогон записью за край РАНЬШЕ, чем тот успевает
    // напечатать хоть одно утверждение, — и «гейт красный» читается как «тест сломан».
    game::Instance buf[N + GUARD];
    for (uint32_t i = 0; i < N + GUARD; ++i) buf[i].x = -1.0f;
    game::InstanceStage stage(buf, N);
    game::Instance one{};
    one.x = 7.0f;

    stage.begin();
    for (uint32_t i = 0; i < N + 5; ++i) stage.push(one);
    check(stage.count() == N, "the stage stops at its capacity");
    check(stage.dropped() == 5, "the stage counts what it refused");
    check(stage.data() == buf, "the stage never moved off the buffer it was given");
    check(buf[N - 1].x == 7.0f, "the last accepted instance actually landed");
    check(buf[N].x == -1.0f && buf[N + GUARD - 1].x == -1.0f,
          "the stage wrote nothing past the capacity it was given");

    // Длина в байтах меряется на НЕПОЛНОМ кадре. На полном она совпадает с ёмкостью, и утверждение
    // прошло бы на накопителе, отдающем ёмкость вместо числа, — то есть на том, который грузит в
    // вершинный буфер мусор за концом кадра. Проверено сломанной реализацией.
    stage.begin();
    for (uint32_t i = 0; i < 3; ++i) stage.push(one);
    check(stage.bytes() == 3 * sizeof(game::Instance), "the byte length follows the count");
    check(stage.capacity() == N, "the capacity is what the stage was handed");

    // `begin` обнуляет и СЧЁТЧИК ОТКАЗОВ тоже: кадр, унаследовавший чужие потери, докладывал бы про
    // переполнение, которого в нём не было, — и первое же настоящее переполнение утонуло бы в шуме.
    stage.begin();
    check(stage.count() == 0 && stage.dropped() == 0, "begin clears the count and the refusals");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    for (uint32_t f = 0; f < 32; ++f) {
        stage.begin();
        for (uint32_t i = 0; i < N; ++i) stage.push(one);
    }
    const long during = framework::probe::allocs;
    framework::probe::in_hot = false;
    std::printf("  stage: %u frames of %u instances = %ld allocations\n", 32u, N, during);
    check(during == 0, "the steady stage frame does not touch the heap");
    check(stage.count() == N, "the counted frames actually filled the stage");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool plain_ok = framework::probe::control::plain_allocation();
    const long plain = framework::probe::allocs;
    framework::probe::allocs = 0;
    const bool aligned_ok = framework::probe::control::aligned_allocation();
    const long aligned = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(plain_ok && plain > 0, "control: the counter sees a plain allocation");
    check(aligned_ok && aligned > 0, "control: the counter sees an over-aligned allocation");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("sprites of the framework into instances of the shooter\n");

    const game::Atlas atlas = game::build_atlas();
    check(game::region_at(atlas, game::RID_Star) != nullptr,
          "control: the atlas the gate reads really holds the cut it asks about");

    test_geometry(atlas);
    test_exposure(atlas);
    test_refusals(atlas);
    test_overflow(atlas);
    test_rotation(atlas);
    test_stage();

    std::printf("game-sprite-out: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
