#pragma once

#include <string>

#include "platformer_replay_io.hpp"
#include "platformer_sim.hpp"
#include "platformer_stage_hash.hpp"
#include "verify.hpp"

// Обстановка гейтов реплея: как прогон образца ЗАПИСЫВАЕТСЯ и как он переигрывается. Отдельно от
// утверждений по той же границе, что у гейтов отката: здесь то, ЧТО гоняют, там то, что про это
// утверждают, — и оба гейта вертикали 2 (поток в памяти и тот же поток файлом) гоняют одно и то же.
//
// Маршрут берётся у `platformer_sim` — тот самый, чей хеш прибит голденом: реплей обязан описывать
// маршрут ИГРЫ, а своя последовательность нажатий проверяла бы верификатор на прогоне, которого не
// знает никто.
namespace platformer::replay_run {

using Stream = replay_io::Stream;

// Возвращает false ровно на «уровень не загрузился»: у гейта на это своё слово, и `check` внутри
// обстановки означал бы, что она судит вместо него.
inline bool record(const std::string& bundle, Stream& s) {
    Stage st;
    if (!claim_format_holds() || !load_stage(bundle, st)) return false;
    StageReplaySim sim{{&st}};
    s.reset(1);
    const uint32_t n = script_ticks();
    for (uint32_t t = 0; t < n; ++t) {
        const ch::MoveInput in = script_input(t);
        sim.step(&in);
        // Отказ записи даёт КОРОТКИЙ поток, а не пустой, и `record()` о нём молчать не должен:
        // «прогон верифицировался» тогда сказано про хвост, которого в потоке нет.
        if (!s.record(&in, sim.hash())) return false;
    }
    return true;
}

// Уровень грузится ЗАНОВО на каждое переигрывание: реплей отвечает на вопрос «из начального
// состояния и записанного ввода получается то же самое», и сцена, донёсшая до него хвост прошлого
// прогона, отвечала бы на другой.
inline bool replay(const std::string& bundle, const Stream& s, framework::replay::Verdict& out) {
    Stage fresh;
    if (!claim_format_holds() || !load_stage(bundle, fresh)) return false;
    StageReplaySim sim{{&fresh}};
    out = framework::replay::verify(sim, s, 1);
    return true;
}

} // namespace platformer::replay_run
