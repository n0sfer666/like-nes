#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#include "platformer_peer.hpp"
#include "platformer_peer_budget.hpp"
#include "platformer_peer_channel.hpp"
#include "platformer_peer_deafness.hpp"
#include "platformer_replay_record.hpp"
#include "platformer_scene.hpp"

// Состояние одного пира и его продвижение по ПОДТВЕРЖДЁННОМУ вводу (спека #22, шаг C).
//
// Отдельно от прогона по той же причине, по которой раньше отделился канал: там цикл и его исходы,
// здесь политика «насколько далеко вперёд позволено уйти и когда перестать ждать дыру». Пир,
// державший обе, дорос до 254 строк при жёстком пороге 250 — и это не про счётчик, а про то, что
// дальше в него дописывали бы обе.
namespace platformer {

// Сколько пир ждёт дыру, прежде чем перестать. Заведомо длиннее любого окна глухоты гейта 7
// (500 мс) и заведомо короче дедлайна (20 с): молчание собеседника лечится ожиданием, а
// форсированный тик — это отказ от сходимости, и путать их нельзя.
//
// В МИЛЛИСЕКУНДАХ, а не в итерациях внешнего цикла, потому что спорит он с величинами
// СТЕННОГО времени — окном глухоты снизу и дедлайном сверху, — а цена итерации задана
// гранулярностью СНА ОС. Две тысячи проходов с `sleep_for(1ms)` — это 2.4 с на macOS и 31 с на
// Windows, где сон округляется вверх до ~15.6 мс; гейт краснел на windows-раннере вторым прогоном
// именно так: терпение переросло дедлайн, и получатель отдавал `code=4` вместо `forced=1`.
constexpr int64_t HOLE_PATIENCE_MS = 3000;

using Clock = std::chrono::steady_clock;

inline int64_t ms_since(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

struct Peer {
    Stage stage;
    replay_record::RecordingSim sim;
    replay_record::RecordingSession ses;
    channel::Channel ch;
    std::vector<uint8_t> have;
    uint32_t known = 0;   // тиков подряд с начала, чей ввод известен
    uint32_t highest = 0; // 1 + номер самого дальнего принятого тика
    uint32_t tick = 0;
    uint32_t queued = 0;
    uint32_t forced = 0;
    Clock::time_point stalled_since{};
    Deafness deaf;
    // Цена прохода раздельно: шаг симуляции с откатами и записью против обслуживания сокета
    // (`platformer_peer_budget.hpp`). Гейт 8 обязан мерить ТОТ ЖЕ бинарь, что ведёт запись.
    budget::Meter sim_cost;
    budget::Meter net_cost;
};

// Курсор монотонный: после отказа ждать дыру он ставится за неё и оттуда же идёт дальше.
inline void advance_known(Peer& p) {
    while (p.known < p.have.size() && p.have[p.known] != 0) ++p.known;
}

// Догнать известный ввод ЦЕЛИКОМ, а не на один тик за проход. Не оптимизация: сессия отвергает
// подтверждение дальше `LEAD` тиков от собственной головы (`too_far`), а второй раз надёжный слой
// его не пришлёт — дедупликация уже посчитала датаграмму принятой. Пир, шагающий по тику за проход
// и вычитывающий за тот же проход всю очередь, отстаёт от неё ровно настолько, насколько машина
// загружена, — и на двенадцатом прогоне подряд отставание перевалило за LEAD, ввод пропал, а
// процессы разошлись. Догоняя до горизонта предсказания, пир держит разрыв структурно.
inline uint32_t catch_up(Peer& p, uint32_t total) {
    uint32_t moved = 0;
    while (p.tick < total && p.tick < p.known + PEER_PREDICT) {
        {
            const budget::Span span(p.sim_cost);
            p.ses.advance(p.sim);
        }
        ++p.tick;
        ++moved;
    }
    return moved;
}

// Дыра ВПЕРЕДИ: подтверждения идут, а одного посередине нет и уже не будет. Ждать его вечно значило
// бы повесить прогон там, где спека требует заявленного исхода. Молчащий собеседник дыры не даёт
// (`highest` стоит вместе с `known`) и ожидания не прерывает.
//
// Часы пускаются НА ПРОХОДЕ внешнего цикла, а не на шаге: внутри догоняющего цикла тот же
// порог объявил бы дырой обычную перестановку датаграмм.
inline void give_up_on_a_hole(Peer& p) {
    if (p.tick < p.known + PEER_PREDICT || p.highest <= p.known) {
        p.stalled_since = Clock::time_point{};
        return;
    }
    if (p.stalled_since == Clock::time_point{}) {
        p.stalled_since = Clock::now();
        return;
    }
    if (ms_since(p.stalled_since) < HOLE_PATIENCE_MS) return;
    p.known = p.highest;
    advance_known(p);
    ++p.forced;
    p.stalled_since = Clock::time_point{};
}

} // namespace platformer
