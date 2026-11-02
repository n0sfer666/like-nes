#pragma once
#include <cstdint>

#include "ent_id.hpp"
#include "fixmath.hpp"
#include "pendulum.hpp"

namespace framework::brawl {

using core::EntId;

// Трос (спека #25, В8в) — маятник `physics::Pendulum` с точкой опоры `rope` уровня, седоком и
// остыванием после отпускания.
struct RopeState {
    physics::Pendulum swing;
    EntId rider;
    uint8_t cooldown = 0;
};

// Досягаемость — от рук (верх корпуса) до конца троса. Раскачка — ускорение вдоль дуги от полного
// отклонения стика, затухание — доля скорости за тик: с ним энергия без ввода не растёт и на
// округлении. Остывание — тики после отпускания, в которые тот же трос не берётся.
inline constexpr fix32 ROPE_REACH = fix32::from_int(16);
inline constexpr fix32 ROPE_PUMP = fix32::from_raw(1024);
inline constexpr int32_t ROPE_DAMPING = 2048;
inline constexpr uint8_t ROPE_COOLDOWN = 12;

} // namespace framework::brawl
