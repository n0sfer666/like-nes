#include "push.hpp"

#include "support.hpp"

namespace framework::character {
namespace {

constexpr physics::BodyId NOBODY{physics::BodyId::INVALID};

// Кто из задетых толкает. Обход отдаёт ВСЕХ, кого персонаж задевает, и выбирается из них тот, кто
// въезжает глубже прочих. Одного касания мало, и это не запас: свип отвечает ближайшим, а на
// нулевом пути ближайших столько же, сколько касаний, и разводит их ключ — то есть РАСКЛАДКА. Ровно
// этим первая версия сноса и промахивалась мимо исходной находки: персонаж СТОИТ на движущейся
// платформе (она же опора, ключ меньше), сбоку въезжает вторая — ответом приходила опора, то
// единственное тело, которое из толкателей исключено, и сноса не случалось вовсе.
struct Shove {
    const CollisionScene* scene = nullptr;
    physics::BodyId carried = NOBODY;
    fix32 dt;
    fix32 into{};
    Vec2 normal{};
    physics::BodyId body = NOBODY;
    uint32_t key = 0;
};

void weigh(void* user, const physics::RayHit& contact) {
    Shove& best = *static_cast<Shove*>(user);
    if (best.carried.valid() && contact.body.index == best.carried.index) return;
    const fix32 into = dot(support_velocity(*best.scene, contact.body) * best.dt, contact.normal);
    // Ноль отдают статика, замершее тело и тело, уезжающее ОТ персонажа: сносить в этих трёх
    // случаях нечем, и разводить их вторым ответом незачем.
    if (into.raw <= 0) return;
    // Ничью разводит КЛЮЧ, а не порядок обхода: полоса кандидатов идёт в порядке раскладки, и
    // «первый из равных» разошёлся бы сам с собой на перетасованном создании (гейт 2 спеки #15).
    if (best.body.valid() && !(best.into < into) && !(into == best.into && contact.key < best.key))
        return;
    best.into = into;
    best.normal = contact.normal;
    best.body = contact.body;
    best.key = contact.key;
}

struct Wall {
    physics::BodyId mover = NOBODY;
    Vec2 shove{};
    bool met = false;
};

void oppose(void* user, const physics::RayHit& contact) {
    Wall& w = *static_cast<Wall*>(user);
    if (contact.body.index == w.mover.index) return;
    if (dot(contact.normal, w.shove).raw < 0) w.met = true;
}

// Есть ли на новом месте что-то, что сносу ПРОТИВОСТОИТ. Спрашивается направлением, а не самим
// фактом касания: персонажа, которого платформа везёт вдоль стены, касается и она, и его
// собственный пол, и «здесь кто-то есть» объявляло бы вжатым всякого, кто едет мимо чего угодно.
// Вжимает только то, чья нормаль смотрит НАВСТРЕЧУ сносу.
//
// Оба источника спрашиваются по отдельности, а не слиянием `cast_nearest`: слияние отвечает одним
// ближайшим, и стена пряталась бы за собственным толкателем персонажа. Сетка при этом отвечает
// одним тайлом — ближайшим, — и это названная граница: тайл, противостоящий сносу ИЗ-ЗА другого,
// сюда не попадёт, и разберёт его обычным порядком `move_and_slide`. Практического зазора тут нет:
// зазор `SKIN` крупнее допуска касания, поэтому пол под ногами в ответ не попадает вовсе, а в
// ответе оказывается ровно то, во что снос персонажа вдавил.
bool pinned(const CollisionScene& s, const CharacterHull& hull, Vec2 at, Vec2 shove,
            physics::BodyId mover) {
    if (s.grid != nullptr) {
        tilemap::TileHit t;
        if (tilemap::shapecast(*s.grid, hull.shape, at, fix32{}, {}, s.tiles, t) &&
            dot(t.normal, shove).raw < 0)
            return true;
    }
    if (s.world == nullptr) return false;
    physics::QueryFilter f;
    f.mask = hull.mask;
    Wall w{mover, shove, false};
    physics::each_contact(*s.world, hull.shape, at, fix32{}, f, &oppose, &w);
    return w.met;
}

bool push_by_movers(const CollisionScene& s, const CharacterHull& hull, physics::BodyId carried,
                    fix32 dt, Vec2& position) {
    // Толкатель — только ТЕЛО: сдвинуть слой тайлов нечем (`support.hpp`), поэтому сетка здесь не
    // спрашивается вовсе, и тик на уровне без тел не платит за снос ни одного запроса.
    if (s.world == nullptr) return true;
    physics::QueryFilter f;
    f.mask = hull.mask;
    Shove best;
    best.scene = &s;
    best.carried = carried;
    best.dt = dt;
    physics::each_contact(*s.world, hull.shape, position, fix32{}, f, &weigh, &best);
    if (!best.body.valid()) return true;
    const Vec2 shove = best.normal * best.into;
    // Снос АТОМАРЕН — как и перенос опорой, и по той же причине: снос «сколько влезло» вдавливал бы
    // персонажа в стену понемногу каждый тик, то есть за десяток тиков сквозь неё.
    if (pinned(s, hull, position + shove, shove, best.body)) return false;
    position = position + shove;
    return true;
}

} // namespace

bool moved_by_world(const CollisionScene& s, const CharacterHull& hull, physics::BodyId support,
                    bool standing, fix32 dt, Vec2& position) {
    // Порядок половин несущий: перенос СНИМАЕТ перекрытие с опорой, набежавшее за ход этого кадра,
    // и снос после него видит уже восстановленный зазор. В обратном порядке снос читал бы это
    // перекрытие как чужой наезд и добавлял бы к переносу второй ход той же платформы.
    const bool squeezed = standing && !carry_by_support(s, hull, support, dt, position);
    const bool shoved = push_by_movers(s, hull, standing ? support : NOBODY, dt, position);
    // Обе половины зовутся ВСЕГДА, в том числе после отказа первой: вжатый переносом персонаж
    // остаётся стоять там, где стоял, и въехавшая в него сбоку платформа обязана быть увидена.
    return shoved && !squeezed;
}

} // namespace framework::character
