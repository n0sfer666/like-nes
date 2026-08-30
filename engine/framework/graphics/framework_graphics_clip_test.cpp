#include <cstdio>

#include "clip.hpp"
#include "platform_args.hpp"

// Гейт ШКАЛЫ клипа (спека #17, вертикаль 1, шаг A): кадр есть функция от номера тика.
//
// Отдельной целью от голдена анимационного состояния, потому что голден отвечает «три ОС сошлись»,
// а не «сошлись на верном»: шкала, сдвинутая на тик, и пинг-понг, потерявший обратную половину,
// дают стабильный хеш на всех трёх машинах.
//
// Каждое утверждение парное. «Одноразовый замирает на последнем кадре» — правда и про клип,
// который просто перестали считать, поэтому рядом стоит тот же клип с флагом цикла, где он обязан
// начаться заново. «Пинг-понг возвращается» — правда и про клип без флага, если шкала считает
// шаги неверно, поэтому рядом стоит он же без пинг-понга.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void same(uint32_t got, uint32_t want, const char* what) {
    if (got == want) return;
    std::printf("  FAIL: %s: got %u, want %u\n", what, got, want);
    ++fails;
}

using namespace framework::graphics;

ClipFrame frames_abc[3] = {{10, 2, 0}, {11, 3, 7}, {12, 1, 0}};
ClipFrame frames_unit[3] = {{20, 1, 0}, {21, 1, 0}, {22, 1, 0}};
ClipFrame frames_one[1] = {{30, 4, 0}};

Clip clip_of(ClipFrame* f, uint16_t n, uint16_t flags) {
    Clip c;
    c.frames = f;
    c.frame_count = n;
    c.flags = flags;
    return c;
}

// 1. Одноразовый клип против циклического НА ОДНОЙ раскладке: первый период у них общий, дальше
// один замирает, другой начинает круг. Порознь ни одно из утверждений не различает верную шкалу и
// шкалу, которая перестала двигаться.
void test_once_vs_loop() {
    const Clip once = clip_of(frames_abc, 3, CLIP_ONCE);
    const Clip loop = clip_of(frames_abc, 3, CLIP_LOOP);
    same(clip_period(once), 6, "period is the sum of durations");
    same(clip_period(loop), clip_period(once), "the loop flag does not change the period");

    const uint16_t want[6] = {0, 0, 1, 1, 1, 2};
    for (uint32_t t = 0; t < 6; ++t) {
        same(clip_frame_at(once, t), want[t], "once: frame at t");
        same(clip_frame_at(loop, t), want[t], "loop: the first period is the same");
    }
    for (uint32_t t = 6; t < 20; ++t) {
        same(clip_frame_at(once, t), 2, "once: rests on the last frame");
        same(clip_frame_at(loop, t), want[t % 6], "loop: starts the circle over");
    }

    check(!clip_finished(once, 5), "once is not finished one tick before the end");
    check(clip_finished(once, 6), "once is finished exactly at the period");
    check(!clip_finished(loop, 600), "a looping clip never finishes");
}

// 2. Пинг-понг. Пара — тот же клип без флага, иначе «идёт вперёд» было бы правдой про обе шкалы.
void test_pingpong() {
    const Clip pp = clip_of(frames_unit, 3, CLIP_PINGPONG | CLIP_LOOP);
    const Clip fwd = clip_of(frames_unit, 3, CLIP_LOOP);
    same(clip_steps(pp), 4, "three frames bounce in four steps");
    same(clip_steps(fwd), 3, "without the flag there are as many steps as frames");
    same(clip_period(pp), 4, "the bounce costs the middle frame twice");

    const uint16_t want[4] = {0, 1, 2, 1};
    for (uint32_t t = 0; t < 12; ++t) {
        same(clip_frame_at(pp, t), want[t % 4], "pingpong: forth and back");
        same(clip_frame_at(fwd, t), static_cast<uint16_t>(t % 3), "forward: never turns back");
    }

    // Края круга не удваиваются: между последним шагом одного круга и первым шагом следующего
    // стоят РАЗНЫЕ кадры. Утверждение отдельное, потому что формула 2n-2 ошибается именно здесь.
    check(clip_frame_at(pp, 3) != clip_frame_at(pp, 4), "the seam of two circles is not a repeat");
}

// 3. Вырожденные клипы. Пинг-понг из одного кадра — не пустая шкала: формула 2n-2 даёт на нём ноль
// шагов, то есть клип, который перестал рисоваться молча.
void test_degenerate() {
    const Clip single_pp = clip_of(frames_one, 1, CLIP_PINGPONG | CLIP_LOOP);
    const Clip single = clip_of(frames_one, 1, CLIP_LOOP);
    same(clip_steps(single_pp), 1, "a single frame bounces in one step");
    same(clip_period(single_pp), 4, "a single frame keeps its own duration");
    for (uint32_t t = 0; t < 9; ++t) {
        same(clip_frame_at(single_pp, t), 0, "a single frame is shown at every tick");
        same(clip_frame_at(single, t), 0, "with no bounce it is the same frame");
    }

    // Два кадра: обратной половине нечего проходить, и пинг-понг обязан совпасть с прямым ходом.
    ClipFrame two[2] = {{40, 1, 0}, {41, 1, 0}};
    const Clip two_pp = clip_of(two, 2, CLIP_PINGPONG | CLIP_LOOP);
    same(clip_steps(two_pp), 2, "two frames have nothing to bounce through");
    for (uint32_t t = 0; t < 6; ++t) {
        same(clip_frame_at(two_pp, t), static_cast<uint16_t>(t % 2), "two frames alternate");
    }

    const Clip empty = clip_of(nullptr, 0, CLIP_LOOP);
    same(clip_steps(empty), 0, "an empty clip has no steps");
    same(clip_period(empty), 0, "an empty clip has no period");
    same(clip_frame_at(empty, 17), 0, "an empty clip answers with frame zero");
    check(clip_finished(empty, 0), "an empty clip is finished from the start");

    // Кадр нулевой длины не занимает ни одного тика: шкала проходит сквозь него.
    ClipFrame skip[3] = {{50, 0, 0}, {51, 2, 0}, {52, 0, 0}};
    const Clip skipped = clip_of(skip, 3, CLIP_LOOP);
    same(clip_period(skipped), 2, "a zero-length frame adds nothing to the period");
    for (uint32_t t = 0; t < 6; ++t) same(clip_frame_at(skipped, t), 1, "only the shown frame");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("graphics clip timeline\n");
    test_once_vs_loop();
    test_pingpong();
    test_degenerate();
    std::printf("framework-graphics-clip: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
