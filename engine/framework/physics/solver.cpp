#include "solver.hpp"

#include "units.hpp"

namespace framework::physics {
namespace {

// Доля тела в накопленном изменении относительной скорости. Умножение, а не деление на сумму
// обратных масс: доля лежит в [0, 1] и насытиться не может, а частное — может, и именно оно
// съедало контакт под тяжёлым телом (см. комментарий к `Contact::normal_impulse`).
fix32 share_of(int64_t lambda, fix32 share) {
    return fix32::from_raw(fix32::sat((lambda * share.raw) >> fix32::SHIFT));
}

// Импульс прикладывается к паре сразу: третий закон Ньютона здесь буквально одна строка, и
// разносить её по двум местам значило бы дать шанс однажды применить только половину.
void apply(Body& a, Body& b, const Contact& c, Vec2 dir, int64_t lambda) {
    a.velocity = a.velocity - dir * share_of(lambda, c.share_a);
    b.velocity = b.velocity + dir * share_of(lambda, c.share_b);
}

// Материалы двух тел комбинируются симметрично, иначе результат зависел бы от того, какое тело
// оказалось `a`, а это — порядок ключей, то есть деталь нумерации. Упругость — минимумом (стекло
// об подушку не звенит), трение — средним геометрическим (общепринятая формула).
fix32 mixed_restitution(const Body& a, const Body& b) {
    return min_fix(a.material.restitution, b.material.restitution);
}

fix32 mixed_friction(const Body& a, const Body& b) {
    return sqrt_fix(a.material.friction * b.material.friction);
}

void prepare(std::vector<Body>& bodies, std::vector<Contact>& contacts) {
    for (Contact& c : contacts) {
        const Body& a = bodies[c.a];
        const Body& b = bodies[c.b];
        c.normal_impulse = 0;
        c.tangent_impulse = 0;
        c.bias = fix32{};
        // Доля — inv_mass / сумма, то есть [0, 1] по построению; у статики она ноль, и решателю не
        // нужна ветка «а вдруг это статика». Пара, где неподвижны оба, получает две нули и
        // пропускается целиком.
        const fix32 inv_sum = a.inv_mass + b.inv_mass;
        c.share_a = inv_sum.raw > 0 ? a.inv_mass / inv_sum : fix32{};
        c.share_b = inv_sum.raw > 0 ? b.inv_mass / inv_sum : fix32{};
        c.friction = mixed_friction(a, b);
        const fix32 vn = dot(b.velocity - a.velocity, c.normal);
        // Сближение — это ОТРИЦАТЕЛЬНАЯ проекция: нормаль смотрит из `a` в `b`, значит тела
        // сходятся, когда `b` движется навстречу. Порог отсекает дребезг покоящейся стопки:
        // под гравитацией контакт каждый кадр набирает микроскорость, и без порога она честно
        // отскакивала бы — стопка «кипит».
        if (vn < -RESTITUTION_CUTOFF) c.bias = mixed_restitution(a, b) * vn;
    }
}

void solve_normal(Body& a, Body& b, Contact& c) {
    const fix32 vn = dot(b.velocity - a.velocity, c.normal);
    // Клампится НАКОПЛЕННАЯ сумма, а не добавка: контакт умеет только отталкивать, но отдельная
    // итерация вправе уйти в минус, компенсируя перебор предыдущей. Запрет минуса на добавке
    // превратил бы решатель в накачку энергии.
    const int64_t want = c.normal_impulse - (static_cast<int64_t>(vn.raw) + c.bias.raw);
    const int64_t total = want > 0 ? want : 0;
    const int64_t delta = total - c.normal_impulse;
    c.normal_impulse = total;
    apply(a, b, c, c.normal, delta);
}

void solve_friction(Body& a, Body& b, Contact& c) {
    const Vec2 rv = b.velocity - a.velocity;
    Vec2 t;
    // Касательная нормализуется тем же способом, что и нормаль контакта: конус Кулона считается в
    // предположении |t| = 1, и раздутая на округлении касательная выпускала бы трение за конус.
    if (!(normalize(rv - c.normal * dot(rv, c.normal), t).raw > 0)) return;

    // Конус Кулона: касательный импульс ограничен долей уже накопленного нормального. Именно
    // накопленного этой же итерацией, а не заданного заранее, — трение о поверхность, которую
    // тело едва касается, обязано быть слабее трения под весом стопки. Обе величины живут в одной
    // шкале (изменение относительной скорости), поэтому отношение между ними — то же самое, что
    // между импульсами: общий множитель 1/сумма обратных масс сократился.
    const int64_t limit = (static_cast<int64_t>(c.friction.raw) * c.normal_impulse) >> fix32::SHIFT;
    const int64_t want = c.tangent_impulse - dot(rv, t).raw;
    const int64_t total = want > limit ? limit : (want < -limit ? -limit : want);
    const int64_t delta = total - c.tangent_impulse;
    c.tangent_impulse = total;
    apply(a, b, c, t, delta);
}

} // namespace

void solve_velocity(std::vector<Body>& bodies, std::vector<Contact>& contacts) {
    prepare(bodies, contacts);
    for (uint32_t it = 0; it < VELOCITY_ITERATIONS; ++it) {
        for (Contact& c : contacts) {
            if (!(c.share_a.raw > 0 || c.share_b.raw > 0)) continue;
            Body& a = bodies[c.a];
            Body& b = bodies[c.b];
            // Нормаль решается ПЕРЕД трением в каждой итерации: предел конуса Кулона берётся из
            // накопленного нормального импульса, и обратный порядок дал бы трение по значению
            // прошлой итерации — на первой из них по нулю, то есть без трения вовсе.
            solve_normal(a, b, c);
            solve_friction(a, b, c);
        }
    }
    for (Body& b : bodies) {
        if (b.type == BodyType::Dynamic) b.velocity = clamp_speed(b.velocity, MAX_SPEED);
    }
}

// Доли и трение берутся из `prepare()`, то есть из `solve_velocity`, — а он по контракту шага идёт
// раньше. Позиционная коррекция, вызванная в одиночку, увидит нулевые доли и честно ничего не
// сдвинет: это лучше, чем считать их второй раз и получить второй источник округления.
void solve_position(std::vector<Body>& bodies, const std::vector<Contact>& contacts) {
    for (uint32_t it = 0; it < POSITION_ITERATIONS; ++it) {
        for (const Contact& c : contacts) {
            const fix32 excess = max_fix(c.penetration - CONTACT_SLOP, fix32{});
            if (!(excess.raw > 0)) continue;
            // Доля от избытка, а не деление на сумму обратных масс: то частное упирается в потолок
            // Q16.16 ровно так же, как импульс, и выталкивало утонувшее тяжёлое тело втрое
            // медленнее обещанного (замер: 8.0 юнита вместо 19.9997 при массе 4096).
            const fix32 amount = excess * POSITION_CORRECTION;
            Body& a = bodies[c.a];
            Body& b = bodies[c.b];
            a.position = a.position - c.normal * (amount * c.share_a);
            b.position = b.position + c.normal * (amount * c.share_b);
        }
    }
}

} // namespace framework::physics
