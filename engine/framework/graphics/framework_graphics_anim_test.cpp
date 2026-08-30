#include <cstdio>

#include "hash_mix.hpp"
#include "platform_args.hpp"
#include "player.hpp"

// ГЕЙТ 1 спеки #17: последовательность тиков -> кадры и события, свёрнутая в один хеш, совпадает
// на трёх ОС. Состояние анимации входит в геймплей (решение 3 спеки), то есть расхождение здесь
// это не «другая картинка», а другая игра.
//
// Свёртка общая с физикой (`hash_mix.hpp`) намеренно: два голдена, сложенные РАЗНЫМИ смешиваниями,
// невозможно сравнивать между собой, а расхождение констант ничем себя не проявляет. Библиотека
// анимаций на физику при этом не завязана — заголовок нужен ЦЕЛИ ТЕСТА, а не модулю.
//
// Один хеш на весь прогон, а не по клипу: вопрос гейта — «сошлись ли машины», и дробить его на
// четыре числа значило бы четыре раза перепинивать одно и то же при любой правке шкалы.
namespace {

using namespace framework::graphics;
using framework::physics::FNV_OFFSET;
using framework::physics::mix;

int fails = 0;

// Раскладка прогона: четыре клипа, у каждого свой класс шкалы, — иначе голден пинил бы один режим
// и молчал бы про остальные три.
ClipFrame walk[4] = {{1, 6, 0}, {2, 6, 11}, {3, 6, 0}, {4, 6, 12}};
ClipFrame hit[3] = {{5, 2, 0}, {6, 3, 21}, {7, 9, 0}};
ClipFrame idle[3] = {{8, 10, 0}, {9, 4, 31}, {10, 10, 0}};
ClipFrame flick[2] = {{11, 1, 41}, {12, 1, 42}};

Clip clip_of(ClipFrame* f, uint16_t n, uint16_t flags) {
    Clip c;
    c.frames = f;
    c.frame_count = n;
    c.flags = flags;
    return c;
}

// Свёртка идёт ТРЕМЯ числами: общее — сам голден, два канальных — его позитивный контроль.
// Разведены они по конкретному промаху: контроль «прогон с другой скоростью даёт другое число»
// пропустил сломанный `clip_frame_at`, который на всех тиках отвечал нулём, — потому что вместе с
// кадром в общую свёртку идут события, и они одни хеш уже разводили. Утверждение «свёртка читает
// состояние» обязано спрашиваться про КАЖДЫЙ канал по отдельности, иначе живой канал прикрывает
// мёртвый.
struct Fold {
    uint64_t full = FNV_OFFSET;
    uint64_t frames = FNV_OFFSET;
    uint64_t events = FNV_OFFSET;
};

Fold run(uint32_t phase) {
    AnimPlayer p[4];
    AnimEvent buf[8];
    Fold f;

    mix(f.full, anim_play(p[0], clip_of(walk, 4, CLIP_LOOP), buf, 8));
    mix(f.full, anim_play(p[1], clip_of(hit, 3, CLIP_ONCE), buf, 8));
    mix(f.full, anim_play(p[2], clip_of(idle, 3, CLIP_PINGPONG | CLIP_LOOP), buf, 8));
    mix(f.full, anim_play(p[3], clip_of(flick, 2, CLIP_LOOP), buf, 8));
    p[1].rate_den = 2;
    // Сдвиг фазы — только для контрольного прогона: голден считается на `phase == 0`, где эта
    // строка не делает ничего.
    p[0].elapsed = phase;

    for (uint32_t tick = 0; tick < 240; ++tick) {
        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t n = anim_step(p[i], buf, 8);
            // В свёртку идут и КАДР, и метки, и признак конца: кадр молчит про потерянное
            // событие, метки молчат про сдвинутую шкалу, а одноразовый клип, замерший на такте
            // раньше, не меняет ни того, ни другого.
            mix(f.full, anim_frame(p[i]));
            mix(f.full, n);
            for (uint32_t k = 0; k < n && k < 8; ++k) mix(f.full, buf[k]);
            mix(f.full, anim_finished(p[i]) ? 1u : 0u);

            mix(f.frames, anim_frame(p[i]));
            mix(f.events, n);
            for (uint32_t k = 0; k < n && k < 8; ++k) mix(f.events, buf[k]);
        }
    }
    return f;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("graphics animation state golden\n");

    const Fold base = run(0);
    std::printf("anim-state hash = 0x%016llx\n", static_cast<unsigned long long>(base.full));

    // Позитивный контроль: тот же прогон, сдвинутый на один тик, обязан дать другое число в
    // КАЖДОМ канале. Хеш, не зависящий от состояния, сходится на трёх ОС лучше любого верного.
    const Fold shifted = run(1);
    if (shifted.frames == base.frames) {
        std::printf("  FAIL: the fold does not read the frame\n");
        ++fails;
    }
    if (shifted.events == base.events) {
        std::printf("  FAIL: the fold does not read the marks\n");
        ++fails;
    }
    if (shifted.full == base.full) {
        std::printf("  FAIL: the golden does not read the animation state\n");
        ++fails;
    }

    std::printf("framework-graphics-anim: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
