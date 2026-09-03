#pragma once

#include "platformer_scene.hpp"
#include "session.hpp"
#include "snapshot.hpp"

// Уровень образца под откатом (спека #22, вертикаль 1): что именно у сцены есть СОСТОЯНИЕ и как оно
// снимается и возвращается.
//
// Отдельным файлом от гейта по той же границе, что у остальной игры: здесь обстановка, там
// утверждения. Шагу C этот адаптер нужен целиком — второй процесс гоняет ровно его.
//
// Изменяемого в `Stage` ровно два поля, и это не сокращение ради дешевизны: сетка, профиль и
// производные от него читаются за шаг и не пишутся, а `lift` — идентификатор тела, выданный один
// раз при загрузке. Снимок, копирующий их, был бы не полнее — он был бы дороже ровно на размер
// уровня, то есть на ту величину, из-за которой откат перестаёт помещаться в кадр.
namespace platformer {

struct StageSnapshot {
    ph::WorldSnapshot world;
    ch::Character hero;
};

// Порядок в `step` — тот же контракт, что у `step_stage`: сначала мир, потом персонаж. Реплей зовёт
// ЭТУ функцию, а не свою копию порядка, поэтому переигранный тик отличается от исходного только
// тем, когда он случился.
struct StageSim {
    using Input = ch::MoveInput;
    using Snapshot = StageSnapshot;

    Stage* stage = nullptr;

    void save(Snapshot& s) const {
        s.world.capture(stage->world);
        s.hero = stage->hero;
    }

    void restore(const Snapshot& s) {
        s.world.apply(stage->world);
        stage->hero = s.hero;
    }

    // Игрок здесь один, и это ОДИН игрок, а не «пока один»: у образца нет второго персонажа, и
    // строка ввода на тик у сессии всё равно своя на каждого. Второй появится вместе со вторым
    // процессом, не раньше.
    void step(const Input* in) { step_stage(*stage, in[0]); }
};

using StageSession = framework::rollback::Session<StageSim>;

} // namespace platformer
