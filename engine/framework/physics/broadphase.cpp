#include "broadphase.hpp"

#include <algorithm>

#include "sweep_order.hpp"
#include "units.hpp"

namespace framework::physics {
namespace {

// Пара тел, ни одно из которых не может двигаться от удара, решателю не нужна: импульс, делённый
// на нулевую сумму обратных масс, всё равно был бы отброшен. Отсекается здесь, а не в решателе, —
// иначе пол из тысячи статических плиток дал бы тысячи пар за кадр на ровном месте.
//
// У пары, где есть ТРИГГЕР, потребитель другой: события, а не решатель, — и прежнее правило отбросило
// бы ровно то, ради чего триггеры заводят. Кинематическая платформа, въезжающая в статическую зону, не
// отвечает на импульс НИ ОДНОЙ стороной, то есть по прежнему правилу «неподвижна», — а событие обязана
// дать. Поэтому у триггерной пары спрашивается не отзывчивость на импульс, а способность
// ПЕРЕМЕЩАТЬСЯ: два статических тела не сдвинутся никогда, их перекрытие известно на загрузке уровня и
// отвечается запросом `overlap`, а не потоком одинаковых событий каждый кадр до конца игры.
bool useless_pair(const Body& a, const Body& b) {
    if (a.trigger || b.trigger) {
        return a.type == BodyType::Static && b.type == BodyType::Static;
    }
    return a.type != BodyType::Dynamic && b.type != BodyType::Dynamic;
}

} // namespace

void Broadphase::reserve(uint32_t capacity) {
    order_.reserve(capacity);
    bounds_.reserve(capacity);
}

uint64_t Broadphase::build(const std::vector<Body>& bodies, std::vector<Pair>& out) {
    const uint32_t n = static_cast<uint32_t>(bodies.size());
    order_.clear();
    bounds_.clear();
    for (uint32_t i = 0; i < n; ++i) {
        order_.push_back(i);
        bounds_.push_back(padded(bounds(bodies[i]), SPECULATIVE_MARGIN));
    }

    // Компаратор обязан быть ПОЛНЫМ порядком, а не просто «по возрастанию x», — обоснование и сам
    // порядок лежат в `sweep_order.hpp` рядом со вторым его потребителем, индексом запросов.
    const auto& bnd = bounds_;
    std::sort(order_.begin(), order_.end(), [&](uint32_t l, uint32_t r) {
        return sweep_before(bnd[l], bodies[l].key, bnd[r], bodies[r].key);
    });

    out.clear();
    // Кандидат засчитывается ПОСЛЕ проверки на выход из полосы и до всех прочих отсечений. Считать
    // до неё значило бы считать один лишний проход на каждое тело — то самое сравнение, которым
    // цикл и обрывается; считать после фильтров получилось бы число принятых пар, которое у нас уже
    // есть. Мерится ровно работа, которую сделала широкая фаза сверх выдачи.
    uint64_t candidates = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t bi = order_[i];
        const fix32 sweep_end = bounds_[bi].max.x;
        for (uint32_t j = i + 1; j < n; ++j) {
            const uint32_t bj = order_[j];
            // Список отсортирован по левому краю, поэтому первое же тело, начинающееся правее
            // правого края текущего, закрывает и всех, кто за ним, — в этом весь выигрыш SAP.
            if (sweep_end < bounds_[bj].min.x) break;
            ++candidates;
            // Фильтр слоёв — первым из трёх отсечений: он не читает ни AABB, ни тип, и снимает пару
            // двумя операциями «и». Правило целиком и обоснование двустороннего согласия — в
            // `filter.hpp`; здесь оно применяется к телам, а не к парам, и это не случайность:
            // отфильтрованная пара обязана исчезнуть ДО узкой фазы, иначе триггерное событие
            // родилось бы на контакте, которого игра запретила.
            if (!layers_agree(bodies[bi].layer, bodies[bi].mask, bodies[bj].layer, bodies[bj].mask)) {
                continue;
            }
            if (useless_pair(bodies[bi], bodies[bj])) continue;
            if (!overlaps(bounds_[bi], bounds_[bj])) continue;

            // Нормализация по ключу, а не по индексу: индекс — порядок создания, и при
            // перетасованном создании та же пара пришла бы в решатель зеркальной, а импульсы
            // накапливаются последовательно, значит зеркальная пара — другой результат.
            const bool a_first = bodies[bi].key < bodies[bj].key;
            const uint32_t a = a_first ? bi : bj;
            const uint32_t b = a_first ? bj : bi;
            out.push_back({a, b, bodies[a].key, bodies[b].key});
        }
    }

    std::sort(out.begin(), out.end(), [](const Pair& l, const Pair& r) {
        if (l.key_a != r.key_a) return l.key_a < r.key_a;
        return l.key_b < r.key_b;
    });
    return candidates;
}

} // namespace framework::physics
