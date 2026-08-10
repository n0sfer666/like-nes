#include "rest.hpp"

#include "fixmath.hpp"

namespace framework::physics {

void RestTracker::reserve(uint32_t capacity) {
    frozen_.reserve(capacity);
    wake_.reserve(capacity);
    ready_.reserve(capacity);
    held_.reserve(capacity);
    anchor_.reserve(capacity);
    anchor_angle_.reserve(capacity);
    tracked_.reserve(capacity);
    settled_.reserve(capacity);
}

void RestTracker::resize(size_t n) {
    // Тело, добавленное после прошлого шага, приходит живым и без истории: якорь ему поставит первый
    // же `settle`, а окно откроется с нуля — замереть раньше, чем простоит своё, оно не может.
    frozen_.resize(n, 0);
    held_.resize(n, 0);
    anchor_.resize(n);
    anchor_angle_.resize(n);
    tracked_.resize(n, 0);
    settled_.resize(n);
}

void RestTracker::wake(uint32_t body, const Islands& islands) {
    if (body >= frozen_.size()) return;
    const uint32_t r = islands.root(body);
    for (uint32_t i = 0; i < static_cast<uint32_t>(frozen_.size()); ++i) {
        if (islands.root(i) != r) continue;
        frozen_[i] = 0;
        held_[i] = 0;
    }
}

void RestTracker::wake_all() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(frozen_.size()); ++i) {
        frozen_[i] = 0;
        held_[i] = 0;
    }
}

bool RestTracker::static_moved(const Body& b, uint32_t i) const {
    if (tracked_[i] == 0) return true;
    return b.position.x.raw != anchor_[i].x.raw || b.position.y.raw != anchor_[i].y.raw ||
           b.angle.raw != anchor_angle_[i].raw;
}

bool RestTracker::stirs(const std::vector<Body>& bodies, const Islands& islands, uint32_t i) const {
    if (bodies[i].type == BodyType::Static) return false;
    return frozen_[i] == 0 || wake_[islands.root(i)] != 0;
}

bool RestTracker::may_freeze(const Body& b) const {
    if (b.type == BodyType::Dynamic) return true;
    return b.velocity.x.raw == 0 && b.velocity.y.raw == 0 && b.angular_velocity.raw == 0;
}

void RestTracker::wake_touched(const std::vector<Body>& bodies, const std::vector<Pair>& pairs,
                               const Islands& islands) {
    resize(bodies.size());
    wake_.assign(frozen_.size(), 0);

    // Сдвинутая статика поднимает ВСЕХ, и спросить точнее здесь не у кого: пары этого кадра посчитаны
    // уже по НОВОЙ раскладке, а те, кто держался за плиту на старом месте, из них ровно поэтому и
    // выпали. Уехавший пол — самый частый случай такой правки, и он же тот, где точный ответ требует
    // помнить прошлую раскладку целиком. Проход по телам за редкое событие — цена, которую здесь
    // платят осознанно.
    for (uint32_t i = 0; i < static_cast<uint32_t>(bodies.size()); ++i) {
        if (bodies[i].type != BodyType::Static || !static_moved(bodies[i], i)) continue;
        wake_all();
        break;
    }

    // Правка тела игрой — вторая дверь (`rest.hpp`). Замершее обязано быть равно копии, снятой при
    // заморозке; разошлось — значит через неконстантную ручку что-то записали, и остров поднимается.
    // Проход по ВСЕМ телам, а не по списку тронутых: список пришлось бы вести самой ручке, то есть
    // платить за КАЖДОЕ чтение мира записью в мир, — а сравнение замершего тела дешевле, чем `bounds`
    // того же тела, который широкая фаза уже посчитала строкой выше.
    for (uint32_t i = 0; i < static_cast<uint32_t>(bodies.size()); ++i) {
        if (frozen_[i] == 0 || !edited(bodies[i], i)) continue;
        wake_[islands.root(i)] = 1;
    }

    // Пробуждение РАСПРОСТРАНЯЕТСЯ: оживший остров сам становится движущимся соседом, и замерший за
    // ним обязан подняться в том же кадре, а не через один — иначе цепочка ящиков разъезжалась бы по
    // кадру за звено, на глазах у игрока. Цикл до неподвижности: каждый проход будит хотя бы один
    // остров, иначе он последний, — то есть потолок здесь ЧИСЛО ОСТРОВОВ проходов по всем парам, а не
    // «столько же, сколько звеньев». Квадрат этот сегодня терпится осознанно; убрать его — значит
    // завести островам список смежности, и он поедет вместе с бюджетом шага в гейт 8.
    bool changed = true;
    while (changed) {
        changed = false;
        for (const Pair& p : pairs) {
            const bool a = stirs(bodies, islands, p.a);
            const bool b = stirs(bodies, islands, p.b);
            if (a == b) continue;
            const uint32_t still = a ? p.b : p.a;
            if (bodies[still].type == BodyType::Static) continue;
            const uint32_t r = islands.root(still);
            if (wake_[r] != 0) continue;
            wake_[r] = 1;
            changed = true;
        }
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(frozen_.size()); ++i) {
        if (wake_[islands.root(i)] == 0) continue;
        frozen_[i] = 0;
        held_[i] = 0;
    }
}

void RestTracker::settle(std::vector<Body>& bodies, const Islands& islands) {
    const uint32_t n = static_cast<uint32_t>(bodies.size());
    resize(n);

    // Окно мерится и замершим телам тоже, хотя их состояние заведомо не менялось. Это не забытая
    // оптимизация: проверка стоит двух вычитаний, а взамен каждый кадр ПЕРЕПОДТВЕРЖДАЕТ, что
    // замершее стоит. Тело, сдвинувшееся в заморозке, — дефект, и обнаружить его обязан тот же
    // механизм, что его заморозил: иначе он проявится расхождением голдена через раунд, без единого
    // указания на причину.
    for (uint32_t i = 0; i < n; ++i) {
        const Body& b = bodies[i];
        if (b.type == BodyType::Static) {
            // Статике якорь ставится не ради окна покоя, а как отметка «здесь она была на конец
            // шага». Сравнение с ней в начале следующего и отвечает на «пол сдвинули?».
            anchor_[i] = b.position;
            anchor_angle_[i] = b.angle;
            tracked_[i] = 1;
            continue;
        }
        const bool inside = abs_fix(b.position.x - anchor_[i].x) < REST_SLOP &&
                            abs_fix(b.position.y - anchor_[i].y) < REST_SLOP &&
                            b.angle.raw == anchor_angle_[i].raw;
        if (inside && held_[i] < REST_FRAMES) {
            ++held_[i];
            continue;
        }
        if (inside) continue;
        held_[i] = 0;
        anchor_[i] = b.position;
        anchor_angle_[i] = b.angle;
    }

    // Решение принимает ОСТРОВ: одно неотстоявшееся тело держит всю группу. Свёртка идёт логическим
    // «и» по представителю, поэтому от порядка обхода не зависит — гейт 2 требует того же результата
    // при перетасованном порядке создания.
    ready_.assign(n, 1);
    for (uint32_t i = 0; i < n; ++i) {
        if (bodies[i].type == BodyType::Static) continue;
        if (held_[i] < REST_FRAMES || !may_freeze(bodies[i])) ready_[islands.root(i)] = 0;
    }
    for (uint32_t i = 0; i < n; ++i) {
        Body& b = bodies[i];
        const bool freeze = b.type != BodyType::Static && ready_[islands.root(i)] != 0;
        const bool froze_now = freeze && frozen_[i] == 0;
        frozen_[i] = freeze ? 1 : 0;
        if (!freeze) continue;
        // Обнуление скорости идёт ДО снятия копии, иначе копия помнила бы остаточные ±2 raw, а тело
        // их уже не имело бы, — и следующий же шаг прочитал бы разницу как правку игры.
        if (b.type == BodyType::Dynamic) {
            b.velocity = Vec2{};
            b.angular_velocity = fix32{};
        }
        if (froze_now) settled_[i] = b;
    }
}

} // namespace framework::physics
