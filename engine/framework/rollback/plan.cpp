#include "plan.hpp"

namespace framework::rollback {

void ReplayPlan::reset(uint32_t depth) {
    head_ = 0;
    from_ = 0;
    pending_ = false;
    depth_ = depth;
    too_deep_ = 0;
}

bool ReplayPlan::note_dirty(Tick t) {
    // Тик ещё не сыгран — переигрывать нечего. Не «успех отката», а его отсутствие: ввод просто
    // приехал вовремя.
    if (t >= head_) return true;
    // Глубина считается от ТЕКУЩЕГО тика, а не от самого раннего плана: план ещё может быть
    // погашен, а кольцо снимков вытесняет по номеру тика и ни про какой план не знает.
    if (head_ - t > depth_) {
        ++too_deep_;
        return false;
    }
    // Побеждает самый ранний. Второй ввод, приехавший тем же кадром на более поздний тик, не
    // вправе сузить откат: переигрывание с более позднего тика прошло бы мимо первого испорченного.
    from_ = pending_ ? (t < from_ ? t : from_) : t;
    pending_ = true;
    return true;
}

void ReplayPlan::rewind() {
    head_ = from_;
    pending_ = false;
}

} // namespace framework::rollback
