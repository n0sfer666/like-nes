#include "prepare.hpp"

#include "axis_terms.hpp"
#include "impulse.hpp"
#include "units.hpp"

namespace framework::physics {
namespace {

// Материалы двух тел комбинируются симметрично, иначе результат зависел бы от того, какое тело
// оказалось `a`, а это — порядок ключей, то есть деталь нумерации. Упругость — минимумом (стекло
// об подушку не звенит), трение — средним геометрическим (общепринятая формула).
fix32 mixed_restitution(const Body& a, const Body& b) {
    return min_fix(a.material.restitution, b.material.restitution);
}

fix32 mixed_friction(const Body& a, const Body& b) {
    return sqrt_fix(a.material.friction * b.material.friction);
}

// Накопленное живёт в шкале ОТНОСИТЕЛЬНОЙ СКОРОСТИ, а не импульса, поэтому при смене геометрии его
// нельзя перенести как есть: тот же импульс при новом `k` даёт другое изменение скорости. Отношение
// k_new/k_prev переводит величину в новую шкалу — без него тёплый старт на повернувшемся теле
// вбрасывает в контакт энергию вместо того, чтобы экономить итерации.
int64_t rescale(int64_t lambda, fix32 prev_k, fix32 next_k) {
    if (lambda == 0 || !(prev_k.raw > 0) || !(next_k.raw > 0)) return 0;
    // Произведение считается в int64 БЕЗ насыщения, поэтому обе величины обязаны быть ограничены до
    // него, а не после: `lambda` держит `MAX_ACCUM` (решатель), `next_k` — потолок fix32. Результат
    // клампится тем же потолком — отношение шкал бывает и больше единицы.
    const int64_t scaled = (lambda * next_k.raw) / prev_k.raw;
    return scaled > MAX_ACCUM ? MAX_ACCUM : (scaled < -MAX_ACCUM ? -MAX_ACCUM : scaled);
}

} // namespace

void prepare_contacts(std::vector<Body>& bodies, std::vector<Manifold>& manifolds) {
    for (Manifold& m : manifolds) {
        const Body& a = bodies[m.a];
        const Body& b = bodies[m.b];
        // Касательная — ФИКСИРОВАННЫЙ перпендикуляр нормали, а не направление скольжения. Взятая из
        // скорости, она переворачивается на околонулевой относительной скорости — знак прыгает
        // кадр через кадр, и накопленное трение прошлого кадра приходит в этот со знаком минус.
        const Vec2 tangent = {-m.normal.y, m.normal.x};
        const fix32 restitution = mixed_restitution(a, b);
        const fix32 friction = mixed_friction(a, b);

        for (uint8_t i = 0; i < m.count; ++i) {
            ManifoldPoint& p = m.points[i];
            const fix32 prev_kn = p.normal.k;
            const fix32 prev_kt = p.tangent.k;
            p.normal = axis_terms(a, b, p.anchor_a, p.anchor_b, m.normal);
            p.tangent = axis_terms(a, b, p.anchor_a, p.anchor_b, tangent);
            p.normal_impulse = rescale(p.normal_impulse, prev_kn, p.normal.k);
            p.tangent_impulse = rescale(p.tangent_impulse, prev_kt, p.tangent.k);

            // Предел конуса переводится в шкалу накопленного отношением k_t/k_n. В вертикали 1 это
            // отношение было тождественной единицей, и его отсутствие ничего не ломало; с вращением
            // плечо входит в нормаль и в касательную по-разному, и без перевода трение либо
            // выпускается за конус, либо душится.
            p.cone = p.normal.k.raw > 0 ? friction * p.tangent.k / p.normal.k : fix32{};

            // Цель отскока считается ОДИН раз, по скорости сближения на входе в шаг. Считать её от
            // текущей скорости внутри итерации нельзя: итерации гасят сближение, цель уезжает вслед
            // за ним, и мяч отскакивает тем ниже, чем больше итераций, — то есть число итераций
            // начинает менять физику, а не только точность.
            const fix32 vn = dot(relative_velocity(a, b, p), m.normal);
            p.bias = vn < -RESTITUTION_CUTOFF ? restitution * vn : fix32{};
        }
    }

    // Тёплый старт — ВТОРЫМ проходом по всему списку, а не сразу за подготовкой каждой пары. Он
    // меняет скорости, а цель отскока читает их же: применённый раньше, он занизил бы упругость
    // контактов, стоящих в списке позже, — и порядок пар начал бы влиять на высоту отскока.
    for (Manifold& m : manifolds) {
        Body& a = bodies[m.a];
        Body& b = bodies[m.b];
        const Vec2 tangent = {-m.normal.y, m.normal.x};
        for (uint8_t i = 0; i < m.count; ++i) {
            const ManifoldPoint& p = m.points[i];
            apply_axis(a, b, p.normal, m.normal, p.normal_impulse);
            apply_axis(a, b, p.tangent, tangent, p.tangent_impulse);
        }
    }
}

} // namespace framework::physics
