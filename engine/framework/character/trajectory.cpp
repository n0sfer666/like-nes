#include "trajectory.hpp"

#include "hash_mix.hpp"

namespace framework::character {

TrajectoryHash::TrajectoryHash() : value(physics::FNV_OFFSET) {}

void TrajectoryHash::feed(const Character& c) {
    // Позиция и скорость идут СЫРЫМИ int32: `to_int()` отбросил бы дробную часть, то есть ровно ту
    // величину, в которой живёт расхождение между платформами. Голден по округлённым до пикселя
    // координатам молчал бы про сдвиг на 1/65536, который через тысячу тиков становится пикселем.
    physics::mix(value, static_cast<uint32_t>(c.position.x.raw));
    physics::mix(value, static_cast<uint32_t>(c.position.y.raw));
    physics::mix(value, static_cast<uint32_t>(c.velocity.x.raw));
    physics::mix(value, static_cast<uint32_t>(c.velocity.y.raw));
    physics::mix(value, static_cast<uint32_t>(c.state));
    physics::mix(value, c.coyote_left);
    physics::mix(value, c.buffer_left);
    // Флаги — одним словом, по биту на флаг: пять отдельных вызовов стоили бы двадцати смешиваний
    // ради пяти бит, а упакованное слово ловит любую их комбинацию тем же одним.
    const uint32_t flags = (c.on_ground ? 1u : 0u) | (c.jump_active ? 2u : 0u) |
                           (c.jump_was_held ? 4u : 0u) | (c.hit_ceiling ? 8u : 0u) |
                           (c.hit_wall ? 16u : 0u);
    physics::mix(value, flags);
}

} // namespace framework::character
