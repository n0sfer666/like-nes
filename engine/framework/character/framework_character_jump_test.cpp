#include <cstdio>

#include "controller.hpp"
#include "framework_character_scene.hpp"
#include "platform_args.hpp"

// Прыжок ЦЕЛЕВОЙ ВЫСОТОЙ: вершина совпадает с `jump_height` профиля, и удержание кнопки этой
// высотой управляет (требования спеки #16, раздел «Прыжок»).
//
// Проверяется юнитами, а не хешем, и это не дублирование голдена: голден отвечает «три ОС сошлись»,
// то есть ловит расхождение платформ, а не ошибку вывода. Неверная формула высоты даёт стабильный
// хеш на всех трёх машинах — и молчит.
//
// Допуск взят НЕ на глаз: дискретная выборка ложится на непрерывную параболу (вывод в
// `profile.cpp`), а вершина параболы почти никогда не приходится на границу тика — ближайшая
// выборка отстоит от неё не больше чем на полтика, и за полтика парабола опускается на g*dt^2/8.
// Это единственный источник недобора, поэтому он и есть допуск.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::character;

fix32 tick_dt() { return fix32::from_int(1) / fix32::from_int(60); }

// Прогон одного прыжка: кнопка удерживается `hold` тиков, дальше отпущена. Возвращает набранную
// высоту — расстояние от стартовой позиции до верхней точки.
fix32 jump_apex(const MoveProfile& p, const MoveDerived& d, uint32_t hold) {
    Scene sc = make_scene();
    const CollisionScene s = sc.view();
    const CharacterHull hull = make_hull();
    Character c = standing_at(fix32{});
    const fix32 start_y = c.position.y;
    fix32 top = start_y;
    for (uint32_t t = 0; t < 240; ++t) {
        MoveInput in;
        in.jump_held = t < hold;
        step(s, hull, p, d, in, tick_dt(), c);
        top = min_fix(top, c.position.y);
        if (t > 0 && c.on_ground) break;
    }
    return start_y - top;
}

void test_target_height() {
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    // Недобор ограничен полутиковым спуском параболы: g*dt^2/8.
    const fix32 band = (p.gravity_rise * tick_dt() * tick_dt()) / fix32::from_int(8);
    const fix32 apex = jump_apex(p, d, 240);
    std::printf("  apex=%.4f target=%.4f band=%.4f\n", apex.to_double(), p.jump_height.to_double(),
                band.to_double());
    check(!(p.jump_height < apex), "the apex never overshoots the target height");
    check(p.jump_height - band - fix32::from_float(0.01) < apex, "the apex reaches the target height");

    // ПОЗИТИВНЫЙ КОНТРОЛЬ полосы. Допуск, в который пролезает и неверный вывод, не проверяет
    // ничего: вывод без поправки на полтика (jump_speed = sqrt(2*g*h)) обязан вылететь за полосу
    // ВВЕРХ, иначе поправка ни на что не влияет и её незачем было выводить.
    MoveDerived wrong = d;
    wrong.jump_speed = d.jump_speed + (p.gravity_rise * tick_dt()) / fix32::from_int(2);
    const fix32 wrong_apex = jump_apex(p, wrong, 240);
    check(p.jump_height + band < wrong_apex, "the uncorrected derivation is rejected by the band");
}

void test_variable_height() {
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    const fix32 band = (p.gravity_rise * tick_dt() * tick_dt()) / fix32::from_int(8);
    const fix32 full = jump_apex(p, d, 240);
    const fix32 tap = jump_apex(p, d, 1);
    const fix32 mid = jump_apex(p, d, 6);
    std::printf("  tap=%.4f mid=%.4f full=%.4f\n", tap.to_double(), mid.to_double(),
                full.to_double());
    // Вершина касания сверяется ПОЛОСОЙ вокруг её собственной формулы, а не порогом «не ниже
    // минимальной высоты». Порог односторонний, и всё, что выше, он пропускает — в том числе
    // прыжок, который отпускание вообще не обрезало: полная высота 64 «не ниже 16» тоже.
    //
    // Формула не равна `min_jump_height`, и это устройство тика, а не погрешность: отпускание
    // наблюдаемо ФРОНТОМ, то есть требует нажатой кнопки в предыдущем тике, поэтому первый тик
    // прыжка успевает проинтегрировать полную скорость подъёма. Отсюда лишние `jump_speed*dt` —
    // при штатном профиле 6.37 юнита сверх шестнадцати. Недобор тот же полутиковый, что у целевой
    // высоты, поэтому и полоса та же.
    const fix32 tap_target = p.min_jump_height + d.jump_speed * tick_dt();
    check(!(tap_target < tap), "a tap never overshoots the height its own release allows");
    check(tap_target - band - fix32::from_float(0.01) < tap, "and it does reach that height");
    check(tap < mid && mid < full, "holding longer jumps higher");
    check(mid < p.jump_height, "a partial hold stays below the target height");
}

void test_ceiling_kills_the_rise() {
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    Scene sc = make_scene();
    const CollisionScene s = sc.view();
    const CharacterHull hull = make_hull();
    // Под потолком сцены: до него меньше, чем целевая высота прыжка, значит подъём обязан
    // оборваться ударом, а не долететь.
    Character c = standing_at(fix32::from_int(-300));
    const fix32 start_y = c.position.y;
    bool touched = false;
    fix32 top = start_y;
    for (uint32_t t = 0; t < 120; ++t) {
        MoveInput in;
        in.jump_held = true;
        step(s, hull, p, d, in, tick_dt(), c);
        touched = touched || c.hit_ceiling;
        top = min_fix(top, c.position.y);
        if (t > 0 && c.on_ground) break;
    }
    check(touched, "the head hit is observable through hit_ceiling");
    check(start_y - top < p.jump_height, "the ceiling cuts the rise short");
    check(c.on_ground, "the character lands back on the floor");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character jump gate\n");
    test_target_height();
    test_variable_height();
    test_ceiling_kills_the_rise();
    std::printf("framework-character-jump: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
