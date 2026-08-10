#pragma once
#include "body.hpp"
#include "contact.hpp"

// Приложение импульса и чтение скорости в точке контакта — общий язык подготовки и итераций
// решателя. Отдельным заголовком, потому что обе стороны обязаны считать это ОДИНАКОВО: подготовка
// меряет скорость сближения для упругости, итерация — для добавки, и разные формулы дали бы
// отскок, зависящий от того, какая из них округлила иначе.
namespace framework::physics {

// Доля тела в накопленном изменении относительной скорости. Умножение на долю, а не деление на `k`:
// доля ограничена по построению, а частное 1/k доходит до тысяч и насыщается задолго до того, как
// контакт разрешён (см. комментарий к `ManifoldPoint::normal_impulse`).
inline fix32 share_of(int64_t lambda, fix32 share) {
    return fix32::from_raw(fix32::sat(fix32::shift_down(lambda * share.raw)));
}

// Скорость ТОЧКИ тела, а не тела: у вращающегося тела они разные, и вся вертикаль 2 про эту разницу.
// Угловая скорость меряется в оборотах, поэтому переводится в радианы здесь — единственном месте,
// где она встречается с длиной.
inline Vec2 point_velocity(const Body& b, Vec2 r) {
    const fix32 omega = TAU * b.angular_velocity;
    return b.velocity + Vec2{-r.y * omega, r.x * omega};
}

inline Vec2 relative_velocity(const Body& a, const Body& b, const ManifoldPoint& p) {
    return point_velocity(b, p.anchor_b) - point_velocity(a, p.anchor_a);
}

// Импульс прикладывается к паре сразу и целиком: третий закон Ньютона здесь буквально четыре
// строки, и разносить их по двум местам значило бы дать шанс однажды применить половину.
inline void apply_axis(Body& a, Body& b, const AxisTerms& t, Vec2 axis, int64_t lambda) {
    a.velocity = a.velocity - axis * share_of(lambda, t.lin_a);
    b.velocity = b.velocity + axis * share_of(lambda, t.lin_b);
    a.angular_velocity = a.angular_velocity - share_of(lambda, t.ang_a);
    b.angular_velocity = b.angular_velocity + share_of(lambda, t.ang_b);
}

} // namespace framework::physics
