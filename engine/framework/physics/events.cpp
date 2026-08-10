#include "events.hpp"

#include <algorithm>

namespace framework::physics {
namespace {

uint64_t pair_id(uint32_t key_a, uint32_t key_b) {
    return (static_cast<uint64_t>(key_a) << 32) | static_cast<uint64_t>(key_b);
}

// Событие несёт САМУЮ ГЛУБОКУЮ точку манифольда, а не первую. Первая — та, которую раньше выдал путь
// узкой фазы, то есть порядок обхода вершин, и на грани об грань она меняется от кадра к кадру, стоит
// телу довернуться на четверть градуса. Самая глубокая определена геометрией, а не перебором, и
// именно она отвечает на вопрос, который игра задаёт событию: где ударило.
ContactEvent to_event(const std::vector<Body>& bodies, const Manifold& m, bool trigger) {
    uint8_t deepest = 0;
    for (uint8_t i = 1; i < m.count; ++i) {
        if (m.points[deepest].penetration < m.points[i].penetration) deepest = i;
    }
    ContactEvent e;
    e.key_a = m.key_a;
    e.key_b = m.key_b;
    e.phase = ContactPhase::Begin;
    e.trigger = trigger;
    e.normal = m.normal;
    // Плечо `anchor_a` посчитано узкой фазой от центра тела `a`, поэтому складывать его можно только
    // с ТОЙ ЖЕ позицией — с той, на которой контакт обнаружен. Отсюда и место вызова в `world.cpp`:
    // до интеграции. Плечо `b` дало бы ту же точку — все три пути узкой фазы кладут в оба плеча один
    // и тот же мировой point, — так что выбор стороны здесь ни на что не влияет.
    e.point = bodies[m.a].position + m.points[deepest].anchor_a;
    e.penetration = m.points[deepest].penetration;
    return e;
}

ContactEvent ended(uint64_t id, bool trigger) {
    // Инициализация значением, а не по умолчанию: обещание «у конца касания геометрии нет» держится
    // на нулях, и опираться здесь на то, что у `fix32` есть свой конструктор по умолчанию, значит
    // сделать чужую деталь несущей. Ноль попадает в накопительный хеш событий — молчаливый мусор
    // разошёлся бы между ОС не физикой, а раскладкой стека.
    ContactEvent e{};
    e.key_a = static_cast<uint32_t>(id >> 32);
    e.key_b = static_cast<uint32_t>(id & 0xffffffffu);
    e.phase = ContactPhase::End;
    e.trigger = trigger;
    return e;
}

} // namespace

void ContactTracker::reserve(size_t pairs) {
    current_.reserve(pairs);
    previous_.reserve(pairs);
}

void ContactTracker::update(const std::vector<Body>& bodies, const std::vector<Manifold>& contacts,
                            const std::vector<Manifold>& resting,
                            const std::vector<Manifold>& triggers,
                            std::vector<ContactEvent>& out) {
    current_.clear();
    for (const Manifold& m : contacts) current_.push_back(to_event(bodies, m, false));
    for (const Manifold& m : resting) current_.push_back(to_event(bodies, m, false));
    for (const Manifold& m : triggers) current_.push_back(to_event(bodies, m, true));
    // Каждый список упорядочен сам по себе, но их СКЛЕЙКА — нет, и сортировка здесь не подстраховка,
    // а восстановление того самого порядка по ключу, ради которого пары и упорядочены. Ключ пары
    // уникален внутри кадра (пара попадает ровно в один из двух списков), поэтому сравнения по нему
    // достаточно: равных элементов нет, и неустойчивость сортировки ничего не решает.
    std::sort(current_.begin(), current_.end(), [](const ContactEvent& l, const ContactEvent& r) {
        return pair_id(l.key_a, l.key_b) < pair_id(r.key_a, r.key_b);
    });

    out.clear();
    // Слияние двух упорядоченных множеств: что есть в обоих — `stay`, что только сейчас — `begin`,
    // что только было — `end`. Слиянием, а не поиском каждой пары в множестве прошлого кадра, потому
    // что порядок событий обязан быть функцией ключей: хеш-множество дало бы верный ОТВЕТ и
    // произвольный ПОРЯДОК, а порядок здесь входит в голден.
    size_t i = 0;
    size_t j = 0;
    while (i < current_.size() || j < previous_.size()) {
        const uint64_t now =
            i < current_.size() ? pair_id(current_[i].key_a, current_[i].key_b) : UINT64_MAX;
        const uint64_t was = j < previous_.size() ? previous_[j].id : UINT64_MAX;

        if (now < was) {
            out.push_back(current_[i]);
            ++i;
        } else if (was < now) {
            out.push_back(ended(previous_[j].id, previous_[j].trigger));
            ++j;
        } else {
            ContactEvent e = current_[i];
            // Пара, сменившая роль между кадрами (игра переключила `trigger` на теле), — это КОНЕЦ
            // одного касания и НАЧАЛО другого, а не продолжение: у игры на контакт и на зону висят
            // разные обработчики, и «stay» с новым флагом дошёл бы до обработчика, который начала не
            // видел.
            if (e.trigger != previous_[j].trigger) {
                out.push_back(ended(previous_[j].id, previous_[j].trigger));
            } else {
                e.phase = ContactPhase::Stay;
            }
            out.push_back(e);
            ++i;
            ++j;
        }
    }

    previous_.clear();
    for (const ContactEvent& e : current_) {
        previous_.push_back({pair_id(e.key_a, e.key_b), e.trigger});
    }
}

} // namespace framework::physics
