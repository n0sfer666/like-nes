#include "player.hpp"

namespace framework::graphics {
namespace {

// Нулевой знаменатель отбивается разбором клипа, поэтому здесь он не политика, а защита от
// деления на ноль: рантайм обязан остаться определённым даже на данных, которые до него доехать
// не могли.
uint32_t rate_den_of(const AnimPlayer& p) { return p.rate_den == 0 ? 1u : p.rate_den; }

uint32_t emit(const Clip& c, uint64_t from_ordinal, uint64_t to_ordinal, AnimEvent* out,
              uint32_t max) {
    const uint32_t steps = clip_steps(c);
    if (steps == 0) return 0;
    uint32_t total = 0;
    for (uint64_t o = from_ordinal; o <= to_ordinal; ++o) {
        const uint16_t frame = step_frame(c, static_cast<uint32_t>(o % steps));
        // Кадр нулевой длины не показывается ни одному тику — и не стреляет. Правило одно на
        // показ и на метку намеренно: разойдись они, клип с таким кадром наносил бы удар, которого
        // игрок не видел. Разбор такой кадр отбивает, так что до рантайма он не доезжает, но
        // тотальным рантайм обязан остаться и на данных, пришедших мимо разбора.
        if (c.frames[frame].duration == 0) continue;
        const AnimEvent ev = c.frames[frame].event;
        if (ev == ANIM_EVENT_NONE) continue;
        if (total < max) out[total] = ev;
        ++total;
    }
    return total;
}

} // namespace

uint64_t anim_time(const AnimPlayer& p) {
    return static_cast<uint64_t>(p.elapsed) * p.rate_num / rate_den_of(p);
}

uint16_t anim_frame(const AnimPlayer& p) { return clip_frame_at(p.clip, anim_time(p)); }

bool anim_finished(const AnimPlayer& p) { return clip_finished(p.clip, anim_time(p)); }

uint32_t anim_play(AnimPlayer& p, const Clip& c, AnimEvent* out, uint32_t max) {
    p.clip = c;
    p.elapsed = 0;
    // Событие ПЕРВОГО кадра срабатывает на запуске, а не на первом шаге: «вошли в кадр» и есть
    // условие метки, и клип, начинающийся с удара, иначе бил бы на тик позже — ровно на тот тик,
    // на котором игрок видит первый кадр.
    const uint64_t first = clip_step_ordinal(p.clip, 0);
    return emit(p.clip, first, first, out, max);
}

uint32_t anim_step(AnimPlayer& p, AnimEvent* out, uint32_t max) {
    if (clip_steps(p.clip) == 0) {
        ++p.elapsed;
        return 0;
    }
    const uint64_t before = clip_step_ordinal(p.clip, anim_time(p));
    ++p.elapsed;
    const uint64_t after = clip_step_ordinal(p.clip, anim_time(p));
    // Полуоткрытый промежуток: шаг, на котором мы уже стояли, повторно не стреляет, а все
    // перескочённые за один тик — стреляют все. Скорость выше единицы перешагивает через кадры
    // целиком, и промолчать о них значило бы терять удары тем сильнее, чем быстрее анимация.
    if (after <= before) return 0;
    return emit(p.clip, before + 1, after, out, max);
}

} // namespace framework::graphics
