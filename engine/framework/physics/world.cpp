#include "world.hpp"

#include "event_hash.hpp"
#include "narrowphase.hpp"
#include "solver.hpp"
#include "state_hash.hpp"

namespace framework::physics {
namespace {

// Демпфирование задано долей, снимаемой ЗА СЕКУНДУ, поэтому за шаг снимается доля от неё. Точная
// экспонента (exp(-d*dt)) здесь не нужна и вредна: она стоила бы ряда или таблицы на горячем пути
// ради разницы, невидимой на шаге 1/60. Кламп множителя снизу — не перестраховка: при демпфировании
// выше 1/dt он ушёл бы в минус, то есть РАЗВОРАЧИВАЛ бы скорость вместо того, чтобы её гасить.
fix32 damping_factor(fix32 damping, fix32 dt) {
    return max_fix(fix32::from_int(1) - damping * dt, fix32{});
}

} // namespace

World::World(uint32_t capacity) {
    // Вся куча — здесь, на конструкторе. Шаг работает в уже выделенном: это и есть гейт 6, и
    // проверяется он не намерением, а счётчиком `operator new` вокруг вызова `step()`.
    const size_t pairs = static_cast<size_t>(capacity) * PAIR_BUDGET_PER_BODY;
    bodies_.reserve(capacity);
    pairs_.reserve(pairs);
    manifolds_.reserve(pairs);
    triggers_.reserve(pairs);
    cache_.reserve(pairs);
    // Событий за кадр может быть больше, чем касаний: пара, распавшаяся в этом кадре, даёт `end`,
    // хотя в текущих списках её уже нет. Потолок — касания этого кадра плюс касания прошлого, то есть
    // удвоенный бюджет пар; резервируется он, а не бюджет, чтобы кадр массового расцепления не стоил
    // аллокации ровно тогда, когда её труднее всего заметить.
    events_.reserve(pairs * 2);
    tracker_.reserve(pairs);
    scratch_.reserve(capacity);
    broad_.reserve(capacity);
    event_hash_ = event_hash_seed();
}

BodyId World::add(const BodyDesc& d) {
    for (const Body& existing : bodies_) {
        if (existing.key == d.key) return BodyId{BodyId::INVALID};
    }
    const BodyId id{static_cast<uint32_t>(bodies_.size())};
    bodies_.push_back(make_body(d));
    return id;
}

void World::step(fix32 dt) {
    for (Body& b : bodies_) {
        if (b.type != BodyType::Dynamic) continue;
        b.velocity = clamp_speed((b.velocity + gravity_ * dt) * damping_factor(b.linear_damping, dt),
                                 MAX_SPEED);
        b.angular_velocity = clamp_fix(b.angular_velocity * damping_factor(b.angular_damping, dt),
                                       -b.max_angular, b.max_angular);
    }

    broad_.build(bodies_, pairs_);

    manifolds_.clear();
    triggers_.clear();
    for (const Pair& p : pairs_) {
        Manifold m;
        if (!collide(bodies_[p.a], bodies_[p.b], m)) continue;
        m.a = p.a;
        m.b = p.b;
        m.key_a = p.key_a;
        m.key_b = p.key_b;
        // Триггер уходит в свой список ДО тёплого старта: манифольд ему строится полный — игре нужны
        // и нормаль, и глубина, — но импульса у него не будет никогда, и накапливать нечего.
        if (bodies_[p.a].trigger || bodies_[p.b].trigger) {
            triggers_.push_back(m);
            continue;
        }
        // Накопленное прошлого кадра вводится ДО решателя и до подготовки: подготовка переводит его
        // в новую шкалу, а решатель начинает с него первую итерацию.
        cache_.carry(m);
        manifolds_.push_back(m);
    }
    // Пары пришли уже отсортированными по (key_a, key_b), а узкая фаза только фильтрует, не
    // переставляет, — значит порядок манифольдов тоже задан ключами. Сортировать второй раз незачем,
    // и это единственная причина, по которой фильтр здесь идёт подряд, а не как попало. На том же
    // порядке стоит двоичный поиск в кеше.

    // События собираются ЗДЕСЬ, до решателя и до интеграции, — и это не свобода расстановки.
    // Полезная нагрузка события состоит из нормали, глубины и ТОЧКИ, а точка хранится плечом от
    // центра тела: сложить плечо, посчитанное узкой фазой, с позицией, которую тело получит ПОСЛЕ
    // интеграции, значит отнести точку касания на пройденный за кадр путь. На 600 юнитах в секунду
    // это десять пикселей — точка уезжает вглубь зоны, мимо грани, о которую ударили.
    // Доставка при этом остаётся послешаговой: `events()` игра читает, когда `step` уже вернулся, —
    // решатель гоняет контакты восемь раз, и реакция игры из середины сдвинула бы мир под
    // оставшимися итерациями.
    tracker_.update(bodies_, manifolds_, triggers_, events_);
    event_hash_ = mix_events(event_hash_, events_);

    solve_velocity(bodies_, manifolds_);

    for (Body& b : bodies_) {
        if (b.type == BodyType::Static) continue;
        b.position = b.position + b.velocity * dt;
        if (b.angular_velocity.raw != 0) set_angle(b, b.angle + b.angular_velocity * dt);
    }

    solve_position(bodies_, manifolds_);

    // Кламп границ мира — последним и для всех подвижных: и интеграция, и позиционная коррекция
    // двигают тело, и вылет за диапазон Q16.16 обернулся бы не «улетел далеко», а насыщением, то
    // есть телом, намертво прилипшим к невидимой стене на 32768.
    for (Body& b : bodies_) {
        if (b.type == BodyType::Static) continue;
        b.position = {clamp_fix(b.position.x, -WORLD_HALF, WORLD_HALF),
                      clamp_fix(b.position.y, -WORLD_HALF, WORLD_HALF)};
    }

    cache_.store(manifolds_);
}

uint64_t World::hash() const { return state_hash(bodies_, scratch_); }

} // namespace framework::physics
