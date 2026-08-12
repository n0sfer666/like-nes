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
    resting_.reserve(pairs);
    scratch_.reserve(capacity);
    broad_.reserve(capacity);
    islands_.reserve(capacity);
    rest_.reserve(capacity);
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
    // Широкая фаза — ПЕРВОЙ, раньше тяготения, и порядок этот несущий. Оживление замерших островов
    // читает пары, а ожившее тело обязано получить своё ускорение В ЭТОМ ЖЕ кадре: отложи тяготение
    // до следующего, и оно потеряло бы кадр свободного падения. Состоянию перестановка безразлична —
    // AABB считается по позиции и повороту, а тяготение меняет скорость.
    // Счётчики обнуляются ЗДЕСЬ, в начале шага, а не по месту первого использования: величина
    // «работа последнего шага» обязана начинаться с нуля целиком, иначе счётчик, чью ветку шаг не
    // прошёл, донёс бы число предыдущего кадра — и гейт прочитал бы его как работу этого.
    counters_.reset();
    counters_.broad_candidates = broad_.build(bodies_, pairs_);
    counters_.pairs = pairs_.size();
    rest_.wake_touched(bodies_, pairs_, islands_);

    for (uint32_t i = 0; i < static_cast<uint32_t>(bodies_.size()); ++i) {
        Body& b = bodies_[i];
        if (b.type != BodyType::Dynamic || rest_.frozen(i)) continue;
        ++counters_.active_bodies;
        b.velocity = clamp_speed((b.velocity + gravity_ * dt) * damping_factor(b.linear_damping, dt),
                                 MAX_SPEED);
        b.angular_velocity = clamp_fix(b.angular_velocity * damping_factor(b.angular_damping, dt),
                                       -b.max_angular, b.max_angular);
    }

    const auto inert = [&](uint32_t i) {
        return bodies_[i].type == BodyType::Static || rest_.frozen(i);
    };

    manifolds_.clear();
    triggers_.clear();
    resting_.clear();
    for (const Pair& p : pairs_) {
        // Пара, обе стороны которой неподвижны, решателю не отдаётся: двигать в ней нечего. Статика
        // неподвижна по типу, замершее тело — по правилу покоя; отличать их здесь незачем. А вот
        // триггер обрабатывается ВСЕГДА и общим путём, даже замершим телом в статической зоне: у
        // зоны нет накопленного импульса, поэтому её манифольд кеш не хранит, и «вспомнить» его было
        // бы неоткуда — пара молча выпала бы в `end`.
        if (!bodies_[p.a].trigger && !bodies_[p.b].trigger && inert(p.a) && inert(p.b)) {
            Manifold m;
            // ЕДИНСТВЕННОЕ место, где сон вообще на что-то влияет: со сном геометрия берётся из
            // кеша, без сна — считается заново. Дальше оба пути обязаны совпасть, и совпадают они не
            // по счастью: тела замерли, значит вход узкой фазы бит в бит тот же, что вчера, а
            // накопленное тёплый старт переносит из того же кеша, из которого его достал бы `recall`.
            //
            // «Что вчера» — про пару, которая вчера УЖЕ была замершей, и `recall` отдаёт только
            // такие (`cache.hpp`). На первом замершем кадре в кеше лежит запись решённой пары, то
            // есть геометрия ДО вчерашней позиционной коррекции, — и путь ниже считает её заново
            // ровно так же, как это сделал бы прогон без сна.
            if (sleep_enabled_ && cache_.recall(p.key_a, p.key_b, m)) {
                ++counters_.recalled;
                resting_.push_back(m);
                continue;
            }
            ++counters_.narrow_checks;
            if (!collide(bodies_[p.a], bodies_[p.b], SPECULATIVE_MARGIN, m)) continue;
            m.a = p.a;
            m.b = p.b;
            m.key_a = p.key_a;
            m.key_b = p.key_b;
            cache_.carry(m);
            resting_.push_back(m);
            continue;
        }
        // Поле у зоны НУЛЕВОЕ, у разрешаемого контакта — спекулятивное, и разница не косметическая.
        // Спекулятивное поле существует затем, чтобы решатель успел затормозить тело до поверхности;
        // зона ничего не тормозит, а её событие отвечает на тот же вопрос, что `overlap`, — «кто
        // здесь сейчас». Считай её тем же полем, что контакт, и `begin` приходил бы телу, стоящему в
        // 1/16 юнита ОТ зоны, с отрицательной глубиной в полезной нагрузке, а `overlap` на той же
        // раскладке отвечал бы «никого»: два ответа на один вопрос, оба правдоподобные.
        const bool trigger = bodies_[p.a].trigger || bodies_[p.b].trigger;
        Manifold m;
        ++counters_.narrow_checks;
        if (!collide(bodies_[p.a], bodies_[p.b], trigger ? fix32{} : SPECULATIVE_MARGIN, m)) continue;
        m.a = p.a;
        m.b = p.b;
        m.key_a = p.key_a;
        m.key_b = p.key_b;
        // Триггер уходит в свой список ДО тёплого старта: манифольд ему строится полный — игре нужны
        // и нормаль, и глубина, — но импульса у него не будет никогда, и накапливать нечего.
        if (trigger) {
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
    // решатель гоняет контакты `VELOCITY_ITERATIONS` раз, и реакция игры из середины сдвинула бы
    // мир под оставшимися итерациями.
    tracker_.update(bodies_, manifolds_, resting_, triggers_, events_);
    event_hash_ = mix_events(event_hash_, events_);

    counters_.velocity_projections = solve_velocity(bodies_, manifolds_, dt);

    for (uint32_t i = 0; i < static_cast<uint32_t>(bodies_.size()); ++i) {
        Body& b = bodies_[i];
        if (b.type == BodyType::Static || rest_.frozen(i)) continue;
        b.position = b.position + b.velocity * dt;
        if (b.angular_velocity.raw != 0) set_angle(b, b.angle + b.angular_velocity * dt);
    }

    counters_.position_projections = solve_position(bodies_, manifolds_);

    // Кламп границ мира — последним и для всех подвижных: и интеграция, и позиционная коррекция
    // двигают тело, и вылет за диапазон Q16.16 обернулся бы не «улетел далеко», а насыщением, то
    // есть телом, намертво прилипшим к невидимой стене на 32768.
    for (uint32_t i = 0; i < static_cast<uint32_t>(bodies_.size()); ++i) {
        Body& b = bodies_[i];
        if (b.type == BodyType::Static || rest_.frozen(i)) continue;
        b.position = {clamp_fix(b.position.x, -WORLD_HALF, WORLD_HALF),
                      clamp_fix(b.position.y, -WORLD_HALF, WORLD_HALF)};
    }

    cache_.store(manifolds_, resting_);

    // Связность и покой — последними, по результату ЭТОГО шага, и порядок между ними задан: решение
    // замереть принимает остров, значит остров должен быть уже посчитан. Оба читают состояние,
    // которое шаг оставил, поэтому следующий кадр начинает оживление с картиной, которой мир
    // действительно кончил, — а не с той, что была до решателя.
    islands_.build(bodies_, manifolds_, resting_);
    rest_.settle(bodies_, islands_);
}

uint64_t World::hash() const { return state_hash(bodies_, scratch_); }

} // namespace framework::physics
