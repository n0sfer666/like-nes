#include "clip.hpp"

namespace framework::graphics {

// Пинг-понг из ОДНОГО кадра — это тот же одиночный кадр, а не 2*1-2 = 0 шагов. Формула без этой
// оговорки даёт пустую шкалу, на которой не стоит ничего, и клип из одного кадра с флагом
// пинг-понга перестаёт рисоваться вовсе — молча, потому что «ноль шагов» это законное состояние
// пустого клипа.
uint32_t clip_steps(const Clip& c) {
    if (c.frame_count == 0) return 0;
    if (c.frame_count == 1 || !clip_pingpongs(c)) return c.frame_count;
    return 2u * c.frame_count - 2u;
}

uint16_t step_frame(const Clip& c, uint32_t step) {
    if (c.frame_count == 0) return 0;
    if (step < c.frame_count) return static_cast<uint16_t>(step);
    // Обратная половина пинг-понга: n-2, n-3, ..., 1. Крайние кадры в неё не входят — иначе на
    // стыке кругов они стояли бы два шага подряд, то есть вдвое дольше выписанной длительности.
    return static_cast<uint16_t>(2u * c.frame_count - 2u - step);
}

uint32_t clip_period(const Clip& c) {
    const uint32_t steps = clip_steps(c);
    uint32_t total = 0;
    for (uint32_t s = 0; s < steps; ++s) total += c.frames[step_frame(c, s)].duration;
    return total;
}

uint64_t clip_step_ordinal(const Clip& c, uint64_t t) {
    const uint32_t steps = clip_steps(c);
    const uint32_t period = clip_period(c);
    if (steps == 0 || period == 0) return 0;

    uint64_t cycle = 0;
    uint64_t rem = t;
    if (clip_loops(c)) {
        cycle = t / period;
        rem = t % period;
    } else if (t >= period) {
        // Одноразовый клип замирает на ПОСЛЕДНЕМ мгновении шкалы, а не за её краем: так «замер»
        // и «доиграл ровно до конца» отвечают одним и тем же шагом, и последний кадр не
        // приходится искать вторым правилом.
        rem = period - 1;
    }

    uint32_t acc = 0;
    for (uint32_t s = 0; s < steps; ++s) {
        const uint32_t d = c.frames[step_frame(c, s)].duration;
        if (rem < acc + d) return cycle * steps + s;
        acc += d;
    }
    return cycle * steps + (steps - 1);
}

uint16_t clip_frame_at(const Clip& c, uint64_t t) {
    const uint32_t steps = clip_steps(c);
    if (steps == 0) return 0;
    return step_frame(c, static_cast<uint32_t>(clip_step_ordinal(c, t) % steps));
}

bool clip_finished(const Clip& c, uint64_t t) {
    // Пустая шкала доиграна ВСЕГДА, и флаг цикла её не спасает: крутить нечего, а «вечно играет»
    // на клипе без единого показанного кадра значило бы, что машина состояний анимаций никогда с
    // него не уйдёт. Порядок проверок здесь и есть это правило.
    const uint32_t period = clip_period(c);
    if (period == 0) return true;
    if (clip_loops(c)) return false;
    return t >= period;
}

} // namespace framework::graphics
