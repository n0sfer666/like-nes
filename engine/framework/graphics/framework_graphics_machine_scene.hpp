#pragma once

#include <cstdio>

#include "machine.hpp"

// Общая раскладка двух гейтов машины. Разложена отдельно, потому что гейты разделены по КЛАССУ
// ОТКАЗА — «выбрала не тот переход» и «переключилась не в тот тик», — а данные у них одни и те же.
namespace scene {

using namespace framework::graphics;

enum : AnimFlags {
    MOVING = 1u << 0,
    HURT = 1u << 1,
    GROUND = 1u << 2,
};

enum : AnimStateId { IDLE = 0, WALK = 1, HIT = 2 };

// idle: два кадра по четыре тика, цикл. Длинный кадр нужен, чтобы «докрутить до конца кадра»
// отличалось от «переключиться сейчас» больше чем на ноль тиков.
inline constexpr ClipFrame IDLE_FRAMES[2] = {{1, 4, ANIM_EVENT_NONE}, {2, 4, ANIM_EVENT_NONE}};
inline constexpr ClipFrame WALK_FRAMES[3] = {{11, 1, 51}, {12, 1, ANIM_EVENT_NONE}, {13, 1, 53}};
inline constexpr ClipFrame HIT_FRAMES[3] = {{21, 1, 61}, {22, 1, ANIM_EVENT_NONE}, {23, 1, 63}};

inline constexpr Clip IDLE_CLIP{IDLE_FRAMES, 2, CLIP_LOOP};
inline constexpr Clip WALK_CLIP{WALK_FRAMES, 3, CLIP_LOOP};
inline constexpr Clip HIT_CLIP{HIT_FRAMES, 3, CLIP_ONCE};

inline int fails = 0;

inline void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

inline void same(uint32_t got, uint32_t want, const char* what) {
    if (got == want) return;
    std::printf("  FAIL: %s: got %u, want %u\n", what, got, want);
    ++fails;
}

// Машина из трёх состояний, у которой переходы задаёт вызывающий: каждый гейт строит свой набор.
struct Rig {
    AnimStateDef states[3]{};
    AnimMachine m{};

    Rig(const AnimTransition* idle_t, uint16_t idle_n, uint16_t idle_flags) {
        states[IDLE] = AnimStateDef{IDLE_CLIP, idle_t, idle_n, idle_flags};
        states[WALK] = AnimStateDef{WALK_CLIP, nullptr, 0, ANIM_STATE_INTERRUPTIBLE};
        states[HIT] = AnimStateDef{HIT_CLIP, nullptr, 0, ANIM_STATE_INTERRUPTIBLE};
        m.states = states;
        m.state_count = 3;
    }

    uint32_t start(AnimStateId id) { return machine_start(m, id, nullptr, 0); }
    uint32_t step(AnimFlags flags) { return machine_step(m, flags, nullptr, 0); }

    // Номер тика, на котором машина ушла из состояния `from`, или `limit`, если не ушла.
    uint32_t leaves_at(AnimStateId from, AnimFlags flags, uint32_t limit) {
        start(from);
        for (uint32_t i = 1; i <= limit; ++i) {
            step(flags);
            if (m.current != from) return i;
        }
        return limit;
    }
};

} // namespace scene
