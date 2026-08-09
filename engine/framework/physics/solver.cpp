#include "solver.hpp"

#include "impulse.hpp"
#include "prepare.hpp"
#include "units.hpp"

namespace framework::physics {
namespace {

void solve_normal(Body& a, Body& b, const Manifold& m, ManifoldPoint& p) {
    const fix32 vn = dot(relative_velocity(a, b, p), m.normal);
    // Клампится НАКОПЛЕННАЯ сумма, а не добавка: контакт умеет только отталкивать, но отдельная
    // итерация вправе уйти в минус, компенсируя перебор предыдущей. Запрет минуса на добавке
    // превратил бы решатель в накачку энергии.
    const int64_t want = p.normal_impulse - (static_cast<int64_t>(vn.raw) + p.bias.raw);
    // Потолок накопленного — не страховка от «слишком сильного» контакта, а условие того, что
    // произведения ниже по течению (перенос шкалы в `rescale`, конус Кулона) остаются в int64.
    // Смысл величины его и задаёт: относительная скорость двух тел не превышает 2*MAX_SPEED.
    const int64_t total = want > MAX_ACCUM ? MAX_ACCUM : (want > 0 ? want : 0);
    const int64_t delta = total - p.normal_impulse;
    p.normal_impulse = total;
    apply_axis(a, b, p.normal, m.normal, delta);
}

void solve_friction(Body& a, Body& b, const Manifold& m, ManifoldPoint& p) {
    const Vec2 tangent = {-m.normal.y, m.normal.x};
    const fix32 vt = dot(relative_velocity(a, b, p), tangent);
    // Конус Кулона: касательное ограничено долей уже накопленного нормального. Именно накопленного
    // этой же итерацией, а не заданного заранее, — трение о поверхность, которую тело едва
    // касается, обязано быть слабее трения под весом стопки.
    const int64_t raw_limit = (static_cast<int64_t>(p.cone.raw) * p.normal_impulse) >> fix32::SHIFT;
    // Тем же потолком, что и нормаль: конус — это доля от накопленного нормального, и множитель у
    // него собственный (trenie * k_t / k_n), то есть больше единицы бывает.
    const int64_t limit = raw_limit > MAX_ACCUM ? MAX_ACCUM : raw_limit;
    const int64_t want = p.tangent_impulse - vt.raw;
    const int64_t total = want > limit ? limit : (want < -limit ? -limit : want);
    const int64_t delta = total - p.tangent_impulse;
    p.tangent_impulse = total;
    apply_axis(a, b, p.tangent, tangent, delta);
}

} // namespace

void solve_velocity(std::vector<Body>& bodies, std::vector<Manifold>& manifolds) {
    prepare_contacts(bodies, manifolds);
    for (uint32_t it = 0; it < VELOCITY_ITERATIONS; ++it) {
        for (Manifold& m : manifolds) {
            Body& a = bodies[m.a];
            Body& b = bodies[m.b];
            for (uint8_t i = 0; i < m.count; ++i) {
                ManifoldPoint& p = m.points[i];
                // Нулевая эффективная обратная масса — пара, где неподвижны оба: импульс, делённый
                // на неё, никуда не пойдёт, а доли равны нулю. Ветка здесь дешевле, чем восемь
                // итераций умножений на ноль.
                if (!(p.normal.k.raw > 0)) continue;
                // Нормаль решается ПЕРЕД трением в каждой итерации: предел конуса берётся из
                // накопленного нормального, и обратный порядок дал бы трение по значению прошлой
                // итерации — на первой из них по нулю, то есть без трения вовсе.
                solve_normal(a, b, m, p);
                solve_friction(a, b, m, p);
            }
        }
    }
    for (Body& b : bodies) {
        if (b.type != BodyType::Dynamic) continue;
        b.velocity = clamp_speed(b.velocity, MAX_SPEED);
        b.angular_velocity = clamp_fix(b.angular_velocity, -b.max_angular, b.max_angular);
    }
}

// Коэффициенты берутся из `prepare_contacts`, то есть из `solve_velocity`, — а он по контракту шага
// идёт раньше. Позиционная коррекция, вызванная в одиночку, увидит нулевые доли и честно ничего не
// сдвинет: это лучше, чем считать их второй раз и получить второй источник округления.
void solve_position(std::vector<Body>& bodies, const std::vector<Manifold>& manifolds) {
    for (uint32_t it = 0; it < POSITION_ITERATIONS; ++it) {
        for (const Manifold& m : manifolds) {
            for (uint8_t i = 0; i < m.count; ++i) {
                const ManifoldPoint& p = m.points[i];
                const fix32 excess = max_fix(p.penetration - CONTACT_SLOP, fix32{});
                if (!(excess.raw > 0)) continue;
                // Доля от избытка, а не деление на сумму обратных масс: то частное упирается в
                // потолок Q16.16 ровно так же, как импульс, и выталкивало утонувшее тяжёлое тело
                // втрое медленнее обещанного (замер: 8.0 юнита вместо 19.9997 при массе 4096).
                //
                // Величина раздаётся ТЕМИ ЖЕ долями, что и импульс, поэтому коррекция разворачивает
                // тело так же, как развернул бы удар: ящик, задевший угол, отъезжает и доворачивает,
                // а не выталкивается плашмя. Доли не обязаны давать в сумме единицу — вместе с
                // угловыми они по построению снимают ровно `amount` проникновения.
                const int64_t amount =
                    (static_cast<int64_t>(excess.raw) * POSITION_CORRECTION.raw) >> fix32::SHIFT;
                Body& a = bodies[m.a];
                Body& b = bodies[m.b];
                a.position = a.position - m.normal * share_of(amount, p.normal.lin_a);
                b.position = b.position + m.normal * share_of(amount, p.normal.lin_b);
                const fix32 turn_a = share_of(amount, p.normal.ang_a);
                const fix32 turn_b = share_of(amount, p.normal.ang_b);
                if (turn_a.raw != 0) set_angle(a, a.angle - turn_a);
                if (turn_b.raw != 0) set_angle(b, b.angle + turn_b);
            }
        }
    }
}

} // namespace framework::physics
