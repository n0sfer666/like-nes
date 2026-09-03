#pragma once

#include <vector>

#include "platformer_replay_io.hpp"
#include "platformer_stage_hash.hpp"

// Запись АВТОРИТЕТНОГО прогона пира (спека #22, гейт 6, шаг C): что процесс на самом деле сыграл,
// а не что он про это думал снаружи.
//
// Обёрткой симуляции, а не счётчиком в пире, по одной причине: ввод, сыгранный на тике, виден ровно
// в одном месте — в вызове `Sim::step`. Туда кольцо отдаёт и подтверждённый ввод, и ПРЕДСКАЗАНИЕ за
// тот, что не приехал, а пир снаружи знает только первое. Записывать отправленное значило бы
// записывать не тот прогон, который состоялся: у получателя с дырой эти два расходятся ровно на
// дыре, то есть на самом интересном тике.
//
// Номер тика лежит В СНИМКЕ рядом с состоянием сцены. Откат возвращает симуляцию назад через
// `restore`, и счётчик, не поехавший назад вместе с ней, дописывал бы переигранные тики в хвост —
// прогон на 420 тиков выходил бы длиной в 420 плюс всё переигранное. Индекс в векторах — он же
// номер тика, поэтому переигрывание ЗАТИРАЕТ прежнюю строку: авторитетен последний раз, когда тик
// сыгран, и это ровно то, чем сессия считает своё состояние.
namespace platformer::replay_record {

struct RecordingSim {
    using Input = StageSim::Input;

    struct Snapshot {
        StageSim::Snapshot inner;
        uint32_t tick = 0;
    };

    StageSim inner;
    uint32_t tick = 0;
    uint32_t high = 0;
    bool spilled = false;
    std::vector<Input> rows;
    std::vector<uint64_t> claims;

    // Место выписывается один раз и больше не растёт: тик отката — такой же тик, а аллокация в тике
    // запрещена инвариантом 5 фреймворка. Тик за пределами выписанного не молчит и не растит вектор
    // — он поднимает флаг, и поток из такой записи не собирается вовсе.
    void expect(uint32_t ticks) {
        rows.assign(ticks, Input{});
        claims.assign(ticks, 0);
        tick = 0;
        high = 0;
        spilled = false;
    }

    void save(Snapshot& s) const {
        inner.save(s.inner);
        s.tick = tick;
    }

    void restore(const Snapshot& s) {
        inner.restore(s.inner);
        tick = s.tick;
    }

    void step(const Input* in) {
        inner.step(in);
        if (tick >= rows.size()) {
            spilled = true;
            return;
        }
        rows[tick] = in[0];
        claims[tick] = stage_claim(*inner.stage);
        ++tick;
        if (tick > high) high = tick;
    }
};

// Сессия пира параметризуется ЗАПИСЫВАЮЩЕЙ симуляцией, а не оборачивает готовую: снимок отката —
// это `Sim::Snapshot`, и номер тика обязан ехать в нём же, иначе откат вернул бы сцену, а запись
// оставил бы на месте.
using RecordingSession = framework::rollback::Session<RecordingSim>;

// `tick == high` спрашивается наравне с переполнением, хотя сегодня недостижимо: `Session::resolve`
// всегда догоняет голову обратно вперёд. Строки в `[tick, high)` — это ПРЕДСКАЗАНИЕ отката, который
// вперёд не доиграли, и поток унёс бы их как сыгранные, оставшись зелёным у верификатора: он
// переигрывает ровно то, что записано. Инвариант, который держится только устройством соседа,
// записан здесь потому, что сосед менялся уже дважды.
inline bool into_stream(const RecordingSim& r, replay_io::Stream& s) {
    if (r.spilled || r.high == 0 || r.tick != r.high) return false;
    s.reset(1);
    for (uint32_t t = 0; t < r.high; ++t)
        if (!s.record(&r.rows[t], r.claims[t])) return false;
    return true;
}

inline bool write_run(const std::string& path, const RecordingSim& r) {
    replay_io::Stream s;
    return into_stream(r, s) && replay_io::write_file(path, s);
}

} // namespace platformer::replay_record
