#include <cstdio>

#include "camera.hpp"
#include "platform_args.hpp"

// Гейт ТРЯСКИ (шаг C вертикали 1, гейт 5 спеки #17): смещение — чистая функция от (тик, seed),
// затухает по остатку заказа и НЕ ПИШЕТ в состояние камеры.
//
// Отделён от гейта сетки, потому что вопрос другого рода: сетка отвечает «камера не дрожит на
// субпиксельном ходу», тряска — «камера дрожит ровно так, как заказано, и только в отрисовке».
// Оба отказа стабильны, воспроизводимы и на позиции камеры не видны вовсе.
namespace {

using namespace framework;
using namespace framework::graphics;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void same(fix32 got, fix32 want, const char* what) {
    if (got == want) return;
    std::printf("  FAIL: %s: got %.6f (raw %d), want %.6f (raw %d)\n", what, got.to_double(),
                got.raw, want.to_double(), want.raw);
    ++fails;
}

void same_u(uint32_t got, uint32_t want, const char* what) {
    if (got == want) return;
    std::printf("  FAIL: %s: got %u, want %u\n", what, got, want);
    ++fails;
}

fix32 fx(int32_t v) { return fix32::from_int(v); }

// 1. Тряска — функция от тика и seed, а не от истории: два одинаковых заказа дают одинаковые
// смещения, разные seed — разные. Без второй половины «одинаковые» было бы правдой и про тряску,
// которая всегда возвращает ноль.
void test_shake_determinism() {
    Camera a{}, b{}, other{};
    camera_shake(a, 40, fx(2), 7);
    camera_shake(b, 40, fx(2), 7);
    camera_shake(other, 40, fx(2), 8);

    uint32_t differs = 0, moves = 0;
    Vec2 prev = camera_shake_offset(a, 0);
    for (uint64_t t = 0; t < 40; ++t) {
        const Vec2 va = camera_shake_offset(a, t);
        same(camera_shake_offset(b, t).x, va.x, "the same order and tick give the same offset");
        check(abs_fix(va.x).raw <= fx(2).raw && abs_fix(va.y).raw <= fx(2).raw,
              "the offset stays inside the amplitude");
        if (!(camera_shake_offset(other, t) == va)) ++differs;
        if (t > 0 && !(va == prev)) ++moves;
        prev = va;
    }
    check(differs > 30, "a different seed shakes differently on almost every tick");
    // Смещение обязано зависеть ОТ ТИКА, а не только от seed: постоянное по времени смещение —
    // это сдвиг картинки, а не тряска, и все утверждения выше про него тоже верны.
    check(moves > 30, "and the same order shakes differently on almost every tick of its own life");
}

// 2. Тряска затухает и кончается. Пара к «кончается» — тот же тик до её конца, где она обязана быть
// ненулевой: «ноль в конце» — правда и про тряску, которая не работала ни разу.
void test_shake_decay() {
    Camera fresh{}, spent{};
    camera_shake(fresh, 40, fx(2), 3);
    camera_shake(spent, 40, fx(2), 3);
    spent.shake_ticks = 4;

    // «Не шире» само по себе выполняется и для тряски, которая не затухает вовсе: равенство
    // проходит нестрогое сравнение. Поэтому утверждений два — ни одного тика ШИРЕ и почти на
    // каждом СТРОГО уже.
    uint32_t wider = 0, strictly = 0, nonzero = 0;
    for (uint64_t t = 0; t < 40; ++t) {
        const fix32 f = abs_fix(camera_shake_offset(fresh, t).x);
        const fix32 s = abs_fix(camera_shake_offset(spent, t).x);
        if (f.raw != 0) ++nonzero;
        if (f.raw < s.raw) ++wider;
        if (s.raw < f.raw) ++strictly;
    }
    same_u(wider, 0, "a nearly spent shake is never wider than a fresh one on the same tick");
    check(strictly > 30, "and on almost every tick it is strictly narrower");
    check(nonzero > 30, "while a fresh shake actually moves the view");

    Camera over{};
    camera_shake(over, 2, fx(2), 3);
    CameraConfig cfg{};
    camera_follow(over, cfg, Vec2{}, 0);
    camera_follow(over, cfg, Vec2{}, 0);
    same(camera_shake_offset(over, 1).x, fix32{}, "once the order runs out the shake is exactly zero");
}

// 3. Тряска НЕ ПИШЕТ в состояние камеры: та же траектория с заказом и без него даёт бит-в-бит один
// и тот же центр. Это инвариант 1 спеки («рендер не пишет в симуляцию») на самом соблазнительном
// для нарушения месте.
void test_shake_does_not_move_the_camera() {
    CameraConfig cfg{};
    cfg.policies = CAMERA_DEADZONE | CAMERA_SPEED_LIMIT;
    cfg.dead_half = Vec2{fx(2), fx(2)};
    cfg.max_speed = fx(3);

    Camera quiet{}, shaken{};
    // Заказ длиннее прогона намеренно: тряска, кончившаяся внутри цикла, делает вторую
    // половину утверждения («вид всё-таки разный») вакуумной — она сравнивала бы два нуля.
    camera_shake(shaken, 60, fx(5), 11);
    for (int32_t t = 0; t < 30; ++t) {
        const Vec2 target{fx(t * 2), fx(t)};
        camera_follow(quiet, cfg, target, 1);
        camera_follow(shaken, cfg, target, 1);
    }
    same(shaken.center.x, quiet.center.x, "a shaking camera ends up exactly where a quiet one does");
    same(shaken.center.y, quiet.center.y, "on both axes");
    check(!(camera_view_center(shaken, cfg, 3) == camera_view_center(quiet, cfg, 3)),
          "while the VIEW centre of the two does differ");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("graphics camera shake\n");
    test_shake_determinism();
    test_shake_decay();
    test_shake_does_not_move_the_camera();
    std::printf("framework-graphics-shake: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
