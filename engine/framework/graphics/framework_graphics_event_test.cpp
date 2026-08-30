#include <cstdio>

#include "platform_args.hpp"
#include "player.hpp"

// Гейт СОБЫТИЙ анимации (спека #17, вертикаль 1, шаг A): метка кадра доставляется ровно на том
// тике, на котором кадр появился, и ровно столько раз, сколько кадр появлялся.
//
// Отдельной целью от шкалы, потому что класс поломки другой и в логе CI его обязано называть имя
// цели: шкала может быть верной, а метка при этом теряться на перескоке или дублироваться на стыке
// кругов. Событие анимации входит в геймплей (решение 3 спеки), то есть потерянная метка — это не
// пропущенный кадр, а не нанесённый удар.
//
// Каждое утверждение парное. «Сработало один раз» — правда и про метку, которая не сработала
// вовсе, поэтому рядом стоит циклический клип, где она обязана повториться. «Пинг-понг стреляет
// средним кадром дважды» — правда и про сбитый счёт шагов, поэтому рядом стоит прямой ход, где
// дважды не стреляет ничто.
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

ClipFrame frames_hit[3] = {{10, 2, 0}, {11, 3, 7}, {12, 1, 0}};
ClipFrame frames_lead[2] = {{20, 2, 5}, {21, 2, 0}};
ClipFrame frames_unit[3] = {{30, 1, 1}, {31, 1, 2}, {32, 1, 3}};

Clip clip_of(ClipFrame* f, uint16_t n, uint16_t flags) {
    Clip c;
    c.frames = f;
    c.frame_count = n;
    c.flags = flags;
    return c;
}

// Сколько раз событие `want` пришло за `ticks` тиков. Буфер с запасом: усечение проверяется
// отдельным случаем, а здесь оно исказило бы счёт молча.
uint32_t count_events(AnimPlayer& p, uint32_t ticks, AnimEvent want) {
    AnimEvent buf[16];
    uint32_t n = 0;
    for (uint32_t i = 0; i < ticks; ++i) {
        const uint32_t got = anim_step(p, buf, 16);
        for (uint32_t k = 0; k < got && k < 16; ++k) {
            if (buf[k] == want) ++n;
        }
    }
    return n;
}

// 1. Метка ПЕРВОГО кадра стреляет на запуске. Пара — клип, у которого её нет: иначе «стреляет на
// запуске» было бы правдой и про реализацию, которая шлёт что-нибудь всегда.
void test_play_fires_first() {
    AnimEvent buf[4];
    AnimPlayer lead;
    same(anim_play(lead, clip_of(frames_lead, 2, CLIP_ONCE), buf, 4), 1, "the first frame fires");
    same(buf[0], 5, "and it fires its own mark");

    AnimPlayer quiet;
    same(anim_play(quiet, clip_of(frames_hit, 3, CLIP_ONCE), buf, 4), 0,
         "a clip whose first frame carries no mark fires nothing on play");
}

// 2. Одноразовый против циклического: метка середины приходит один раз против круга за периодом.
void test_once_vs_loop() {
    AnimEvent buf[4];
    AnimPlayer once;
    anim_play(once, clip_of(frames_hit, 3, CLIP_ONCE), buf, 4);
    same(count_events(once, 30, 7), 1, "a once clip delivers its mark exactly once");
    check(anim_finished(once), "and it is finished by then");

    AnimPlayer loop;
    anim_play(loop, clip_of(frames_hit, 3, CLIP_LOOP), buf, 4);
    same(count_events(loop, 30, 7), 5, "a looping clip delivers it once per circle");
}

// 3. Пинг-понг проходит средний кадр дважды за круг — и стреляет дважды. Пара — прямой ход.
void test_pingpong() {
    AnimEvent buf[4];
    AnimPlayer pp;
    anim_play(pp, clip_of(frames_unit, 3, CLIP_PINGPONG | CLIP_LOOP), buf, 4);
    same(count_events(pp, 8, 2), 4, "the middle frame is entered twice per circle");

    // Края круга — пара к середине НА ТОМ ЖЕ прогоне: реализация, стреляющая всем подряд вдвое,
    // прошла бы утверждение выше и провалилась бы здесь.
    AnimPlayer edges;
    anim_play(edges, clip_of(frames_unit, 3, CLIP_PINGPONG | CLIP_LOOP), buf, 4);
    same(count_events(edges, 8, 3), 2, "the far frame is entered once per circle");

    AnimPlayer fwd;
    anim_play(fwd, clip_of(frames_unit, 3, CLIP_LOOP), buf, 4);
    same(count_events(fwd, 8, 2), 3, "going forward it is entered once per circle");
}

// 4. Скорость дробью. Быстрее единицы клип перешагивает кадры целиком, и промолчать о них значило
// бы терять удары тем чаще, чем быстрее анимация. Пара — обычная скорость на том же клипе.
void test_rate() {
    AnimEvent buf[8];
    AnimPlayer fast;
    anim_play(fast, clip_of(frames_unit, 3, CLIP_LOOP), buf, 8);
    fast.rate_num = 3;
    same(anim_step(fast, buf, 8), 3, "three steps crossed in one tick deliver three marks");
    same(buf[0], 2, "in the order they were crossed");
    same(buf[1], 3, "second");
    same(buf[2], 1, "and the circle continues");

    AnimPlayer slow;
    anim_play(slow, clip_of(frames_unit, 3, CLIP_LOOP), buf, 8);
    slow.rate_den = 2;
    same(anim_step(slow, buf, 8), 0, "at half speed the first tick crosses nothing");
    same(anim_step(slow, buf, 8), 1, "the second one does");
    same(anim_frame(slow), 1, "and the frame moved exactly one step");
}

// 5. Усечение ВИДНО вызывающему: вернувшееся число больше буфера и есть улика.
void test_truncation() {
    AnimEvent buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    AnimPlayer p;
    anim_play(p, clip_of(frames_unit, 3, CLIP_LOOP), buf, 8);
    p.rate_num = 3;
    same(anim_step(p, buf, 1), 3, "the count is what happened, not what fit");
    same(buf[0], 2, "and the buffer holds the first of them");
    same(buf[1], 0, "the rest is left untouched");
}

// 6. Перемотка равна проигрыванию: состояние проигрывателя это одно число тиков, и хвост прогона,
// начатого с середины, обязан совпасть с хвостом прогона, доигравшего до неё.
void test_seek_equals_play() {
    AnimEvent buf[8];
    const Clip c = clip_of(frames_unit, 3, CLIP_LOOP);

    AnimPlayer whole;
    anim_play(whole, c, buf, 8);
    for (uint32_t i = 0; i < 5; ++i) anim_step(whole, buf, 8);
    const uint32_t tail_whole = count_events(whole, 7, 2);
    const uint16_t frame_whole = anim_frame(whole);

    AnimPlayer seeked;
    anim_play(seeked, c, buf, 8);
    seeked.elapsed = 5;
    same(count_events(seeked, 7, 2), tail_whole, "a seeked player delivers the same tail");
    same(anim_frame(seeked), frame_whole, "and stands on the same frame");
}

// 7. Кадр нулевой длины не показывается ни одному тику и не стреляет. Пара — та же метка на кадре
// нормальной длины, где она обязана прийти.
void test_zero_duration_is_silent() {
    AnimEvent buf[8];
    ClipFrame ghost[2] = {{40, 0, 9}, {41, 2, 0}};
    AnimPlayer p;
    same(anim_play(p, clip_of(ghost, 2, CLIP_LOOP), buf, 8), 0, "an unshown frame fires nothing");
    same(count_events(p, 20, 9), 0, "and it stays silent for the whole run");

    ClipFrame shown[2] = {{40, 1, 9}, {41, 2, 0}};
    AnimPlayer q;
    same(anim_play(q, clip_of(shown, 2, CLIP_LOOP), buf, 8), 1, "the same mark on a shown frame");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("graphics clip events\n");
    test_play_fires_first();
    test_once_vs_loop();
    test_pingpong();
    test_rate();
    test_truncation();
    test_seek_equals_play();
    test_zero_duration_is_silent();
    std::printf("framework-graphics-event: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
