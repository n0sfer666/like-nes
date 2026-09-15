#include "platformer_live_hooks.hpp"

#include <cstdio>

namespace platformer {

bool LiveHooks::input(uint32_t tick, ch::MoveInput& out) {
    const Clock::time_point now = Clock::now();
    const std::chrono::nanoseconds period(tick_period_ns());
    if (!armed_) {
        armed_ = true;
        next_ = now;
    }
    if (now < next_) return false;
    // Долг меряется ДО сдвига курсора: посчитанный после, он занижен ровно на период — порог
    // срабатывал бы на тринадцатом тике при заявленных двенадцати, а печатаемое число было бы на
    // единицу меньше настоящего, то есть врало бы владельцу о величине рывка.
    const int64_t behind = (now - next_) / period;
    next_ += period;
    // Долг сверх потолка СПИСЫВАЕТСЯ, и вслух: сон машины и утащенное мышью окно копят стенное
    // время, не двигая тиков, а отданный залпом долг — это тики, сыгранные без единого опроса
    // клавиатуры. Молчаливое списание владелец прочитал бы как «игра дёрнулась сама».
    //
    // Печатается СУММА списанного, а не только текущий рывок: списание возвращает ТЕМП, но не
    // тики, а дедлайн пира мерится стенными часами. Машина, потерявшая суммарно больше самой
    // сессии, уйдёт кодом 4 при исправной сети — и §14 руководства велит читать код 4 как «сосед
    // ушёл». Сумма в логе есть единственная улика, отличающая эти два случая.
    if (behind > LIVE_DEBT_TICKS) {
        dropped_ += behind;
        std::printf("live: %lld ticks behind the wall clock, that debt is dropped (%lld total)\n",
                    static_cast<long long>(behind), static_cast<long long>(dropped_));
        next_ = now;
    }
    out = in_.read(tick);
    return true;
}

bool LiveHooks::show(const Stage& stage) {
    // События разбираются ПЕРЕД опросом ввода следующего тика и перед вопросом о выходе: и клавиши,
    // и крестик приезжают очередью ОС, и спрошенное до неё описывало бы предыдущий кадр.
    win_.poll();
    if (win_.quit_asked()) return false;
    win_.draw(stage);
    return true;
}

} // namespace platformer
