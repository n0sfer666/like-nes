#include "snapshot.hpp"

#include "world.hpp"

namespace framework::physics {

void WorldSnapshot::capture(const World& w) {
    // Присваивание, а не `= World{…}` и не `swap`: присваивание вектора ПЕРЕИСПОЛЬЗУЕТ уже
    // выписанную ёмкость, если её хватает, и потому второе снятие той же сцены идёт без единой
    // аллокации. Гейт 4 спеки #22 меряет ровно это, поэтому способ копирования здесь несущий.
    bodies_ = w.bodies_;
    manifolds_ = w.manifolds_;
    triggers_ = w.triggers_;
    resting_ = w.resting_;
    islands_ = w.islands_;
    rest_ = w.rest_;
    cache_ = w.cache_;
    tracker_ = w.tracker_;
    events_ = w.events_;
    counters_ = w.counters_;
    event_hash_ = w.event_hash_;
    gravity_ = w.gravity_;
    sleep_enabled_ = w.sleep_enabled_;
    taken_ = true;
}

void WorldSnapshot::apply(World& w) const {
    w.bodies_ = bodies_;
    w.manifolds_ = manifolds_;
    w.triggers_ = triggers_;
    w.resting_ = resting_;
    w.islands_ = islands_;
    w.rest_ = rest_;
    w.cache_ = cache_;
    w.tracker_ = tracker_;
    w.events_ = events_;
    w.counters_ = counters_;
    w.event_hash_ = event_hash_;
    w.gravity_ = gravity_;
    w.sleep_enabled_ = sleep_enabled_;
    // Индекс запросов не восстанавливается, а МЕТИТСЯ протухшим: он кеш, выведенный из тел, и
    // единственное, что про него верно после возврата, — что он описывает не эти тела.
    w.queries_.invalidate();
}

} // namespace framework::physics
