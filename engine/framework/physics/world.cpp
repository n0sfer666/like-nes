#include "world.hpp"

#include "narrowphase.hpp"
#include "solver.hpp"
#include "state_hash.hpp"

namespace framework::physics {

World::World(uint32_t capacity) {
    // Вся куча — здесь, на конструкторе. Шаг работает в уже выделенном: это и есть гейт 6, и
    // проверяется он не намерением, а счётчиком `operator new` вокруг вызова `step()`.
    bodies_.reserve(capacity);
    pairs_.reserve(static_cast<size_t>(capacity) * PAIR_BUDGET_PER_BODY);
    contacts_.reserve(static_cast<size_t>(capacity) * PAIR_BUDGET_PER_BODY);
    scratch_.reserve(capacity);
    broad_.reserve(capacity);
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
        b.velocity = clamp_speed(b.velocity + gravity_ * dt, MAX_SPEED);
    }

    broad_.build(bodies_, pairs_);

    contacts_.clear();
    for (const Pair& p : pairs_) {
        Contact c;
        if (!collide(bodies_[p.a], bodies_[p.b], c)) continue;
        c.a = p.a;
        c.b = p.b;
        contacts_.push_back(c);
    }
    // Пары пришли уже отсортированными по (key_a, key_b), а узкая фаза только фильтрует, не
    // переставляет, — значит порядок контактов тоже задан ключами. Сортировать второй раз незачем,
    // и это единственная причина, по которой фильтр здесь идёт подряд, а не как попало.

    solve_velocity(bodies_, contacts_);

    for (Body& b : bodies_) {
        if (b.type == BodyType::Static) continue;
        b.position = b.position + b.velocity * dt;
    }

    solve_position(bodies_, contacts_);

    // Кламп границ мира — последним и для всех подвижных: и интеграция, и позиционная коррекция
    // двигают тело, и вылет за диапазон Q16.16 обернулся бы не «улетел далеко», а насыщением, то
    // есть телом, намертво прилипшим к невидимой стене на 32768.
    for (Body& b : bodies_) {
        if (b.type == BodyType::Static) continue;
        b.position = {clamp_fix(b.position.x, -WORLD_HALF, WORLD_HALF),
                      clamp_fix(b.position.y, -WORLD_HALF, WORLD_HALF)};
    }
}

uint64_t World::hash() const { return state_hash(bodies_, scratch_); }

} // namespace framework::physics
