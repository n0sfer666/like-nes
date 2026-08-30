#pragma once

#include "player.hpp"

// Стейт-машина анимаций (спека #17, решение 2: «стейт-машина — тоже данные»). Состояния и переходы
// лежат массивами и приезжают из бандла; здесь только правило выбора и время.
//
// Условия НЕ знают про `MoveState` из #16, хотя требование написано про него: слой графики,
// зависящий от слоя персонажа, развернул бы стрелку зависимостей между модулями ради перечисления
// из трёх значений. Вместо этого условие читает РАЗРЯДЫ, а игра сама раскладывает по ним что угодно —
// состояние движения, «оружие поднято», «получил урон». Цена названа: соглашение о номере разряда
// живёт у вызывающего.
namespace framework::graphics {

using AnimStateId = uint16_t;
using AnimFlags = uint32_t;

constexpr AnimStateId ANIM_STATE_NONE = 0xffffu;

// Две маски, а не одна: «не на земле» — такое же условие, как «на земле», и без второй маски оно
// выражалось бы только удвоением разрядов на каждый факт.
struct AnimCondition {
    AnimFlags all = 0;
    AnimFlags none = 0;
};

enum AnimStateFlags : uint16_t {
    ANIM_STATE_INTERRUPTIBLE = 0,
    // Не отпускает, пока клип не доиграл до конца ПЕРИОДА. Для зацикленного клипа это конец первого
    // круга, а не «никогда»: иначе непрерываемое состояние на цикле — вечная ловушка, из которой
    // машину не вынет ни одно условие.
    ANIM_STATE_HOLD_UNTIL_DONE = 1u << 0,
    // Докручивает текущий КАДР: переход ждёт тика, на котором кадр и так сменился бы.
    ANIM_STATE_HOLD_UNTIL_FRAME_END = 1u << 1,
};

struct AnimTransition {
    AnimStateId to = ANIM_STATE_NONE;
    AnimCondition when{};
    uint16_t priority = 0;
};

struct AnimStateDef {
    Clip clip{};
    const AnimTransition* transitions = nullptr;
    uint16_t transition_count = 0;
    uint16_t flags = ANIM_STATE_INTERRUPTIBLE;
};

struct AnimMachine {
    const AnimStateDef* states = nullptr;
    uint16_t state_count = 0;
    AnimStateId current = ANIM_STATE_NONE;
    AnimPlayer player{};
};

bool anim_condition_holds(const AnimCondition& c, AnimFlags flags);

// Оба ответа — про СЛЕДУЮЩИЙ тик, потому что машина решает ДО того, как проигрыватель шагнёт.
// Условие, прочитанное по текущему времени, держало бы каждое состояние на тик дольше: последний
// кадр клипа показывался бы дважды, и «докрутить» превращалось бы в «показать лишнее».
bool anim_at_frame_end(const AnimPlayer& p);
bool anim_hold_expired(const AnimPlayer& p);

// Победитель — наибольший приоритет; при равном выигрывает ПЕРВЫЙ по списку. Правило нужно записать:
// «любой из подходящих» на трёх машинах даёт три разные анимации при одинаковом sim-хеше.
AnimStateId machine_pick(const AnimMachine& m, AnimFlags flags);

uint32_t machine_start(AnimMachine& m, AnimStateId to, AnimEvent* out, uint32_t max);
uint32_t machine_step(AnimMachine& m, AnimFlags flags, AnimEvent* out, uint32_t max);
uint16_t machine_frame(const AnimMachine& m);

} // namespace framework::graphics
