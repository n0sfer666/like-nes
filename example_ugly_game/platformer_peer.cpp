#include "platformer_peer.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

#include "platformer_input_wire.hpp"
#include "platformer_peer_budget.hpp"
#include "platformer_peer_channel.hpp"
#include "platformer_peer_deafness.hpp"
#include "platformer_peer_result.hpp"
#include "platformer_replay_record.hpp"
#include "platformer_rollback.hpp"
#include "platformer_sim.hpp"

namespace platformer {
namespace {

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
// Сколько получатель ещё подтверждает после собственного конца. Выйди он сразу — последние
// подтверждения не дошли бы, и отправитель ждал бы их до дедлайна, объявив это отказом сети.
constexpr int64_t GRACE_MS = 1000;

using Clock = std::chrono::steady_clock;

int64_t ms_since(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

const char* role_of(bool sender) { return sender ? "send" : "recv"; }

std::string side_file(const PeerConfig& c, bool mine, const char* ext) {
    return c.prefix + "-" + role_of(mine == c.sender) + ext;
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
void advance_known(Peer& p) {
    while (p.known < p.have.size() && p.have[p.known] != 0) ++p.known;
}

void push_input(Peer& p, const PeerConfig& cfg, uint32_t total) {
    if (p.queued >= total || p.ch.link.unacked() >= PEER_INFLIGHT) return;
    const uint32_t t = p.queued;
    const ch::MoveInput in = script_input(t);
    const bool skip = cfg.drop_tick >= 0 && static_cast<uint32_t>(cfg.drop_tick) == t;
    if (!skip) {
        uint8_t body[input_wire::BYTES];
        if (!input_wire::put(body, t, in) || !p.ch.link.send(body, sizeof(body))) return;
    }
    p.ses.deliver(t, 0, in);
    p.have[t] = 1;
    ++p.queued;
    advance_known(p);
}

// Исходы те же, что у `poll`, свёрнутые по всей очереди: `1` что-то приехало, `0` очередь пуста,
// `-2` сокет непригоден. Последний не сводится к первым двум: отказ шва и молчание собеседника
// лечатся по-разному, и пир, перепутавший их, дождался бы дедлайна вместо своего кода.
int drain(Peer& p) {
    uint8_t payload[net::MAX_PAYLOAD];
    int any = 0;
    for (;;) {
        size_t len = 0;
        const int got = channel::poll(p.ch, payload, sizeof(payload), &len);
        if (got == -2) return -2;
        if (got == 0) return any;
        any = 1;
        uint32_t t = 0;
        ch::MoveInput in;
        if (got < 0 || !input_wire::get(payload, len, t, in) || t >= p.have.size()) continue;
        p.ses.deliver(t, 0, in);
        p.have[t] = 1;
        if (t + 1 > p.highest) p.highest = t + 1;
        advance_known(p);
    }
}

// Догнать известный ввод ЦЕЛИКОМ, а не на один тик за проход. Не оптимизация: сессия отвергает
// подтверждение дальше `LEAD` тиков от собственной головы (`too_far`), а второй раз надёжный слой
// его не пришлёт — дедупликация уже посчитала датаграмму принятой. Пир, шагающий по тику за проход
// и вычитывающий за тот же проход всю очередь, отстаёт от неё ровно настолько, насколько машина
// загружена, — и на двенадцатом прогоне подряд отставание перевалило за LEAD, ввод пропал, а
// процессы разошлись. Догоняя до горизонта предсказания, пир держит разрыв структурно.
uint32_t catch_up(Peer& p, uint32_t total) {
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
void give_up_on_a_hole(Peer& p) {
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

PeerStats tally(const Peer& p) {
    PeerStats s;
    s.ticks = p.tick;
    s.rollbacks = p.ses.rollbacks();
    s.replayed = p.ses.replayed();
    s.forced = p.forced;
    s.conflicts = p.ses.conflicts();
    s.too_deep = p.ses.too_deep();
    s.too_far = p.ses.too_far();
    s.resent = p.ch.link.resent();
    s.deaf_from = p.deaf.from();
    s.deaf_to = p.deaf.to();
    s.deaf_stalls = p.deaf.stalls();
    return s;
}

} // namespace

int run_peer(const PeerConfig& cfg) {
    auto p = std::make_unique<Peer>();
    // Формат заявки спрашивается ДО уровня и до сокета: разъехавшись, он сделал бы все заявки пира
    // одним числом, и записанный прогон верифицировался бы у кого угодно с любым вводом.
    if (!claim_format_holds()) {
        std::printf("peer %s: the claim format does not match its own length\n",
                    role_of(cfg.sender));
        return 8;
    }
    if (!load_stage(cfg.bundle, p->stage)) {
        std::printf("peer %s: the level did not load from %s\n", role_of(cfg.sender),
                    cfg.bundle.c_str());
        return 2;
    }
    p->sim.inner.stage = &p->stage;
    p->ses.reset(1, PEER_DEPTH);
    const uint32_t total = script_ticks();
    p->sim.expect(total);
    p->have.assign(total, 0);
    if (!channel::connect(p->ch, side_file(cfg, true, ".port"), side_file(cfg, false, ".port"),
                          PEER_RENDEZVOUS_MS))
        return 3;

    p->deaf.arm(cfg.deaf_at, cfg.deaf_ms);
    const Clock::time_point started = Clock::now();
    Clock::time_point linger_since{};
    bool lingering = false;
    for (uint32_t it = 0;; ++it) {
        if (p->tick >= total && p->known >= total && p->ch.link.unacked() == 0) {
            if (cfg.sender) break;
            if (!lingering) {
                lingering = true;
                linger_since = Clock::now();
            } else if (ms_since(linger_since) >= GRACE_MS) {
                break;
            }
        }
        if (ms_since(started) > PEER_DEADLINE_MS) {
            std::printf("peer %s: gave up at tick %u of %u (known=%u)\n", role_of(cfg.sender),
                        p->tick, total, p->known);
            return 4;
        }
        if (cfg.sender) push_input(*p, cfg, total);
        int got = 0;
        {
            const budget::Span span(p->net_cost);
            channel::flush(p->ch, it);
            p->deaf.update(p->tick);
            if (!p->deaf.running()) got = drain(*p);
        }
        if (got < 0) {
            std::printf("peer %s: the socket went bad at tick %u of %u\n", role_of(cfg.sender),
                        p->tick, total);
            return 6;
        }
        const bool moved = catch_up(*p, total) != 0;
        if (!moved) give_up_on_a_hole(*p);
        if (!moved) p->deaf.stalled();
        if (got == 0 && !moved) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    p->ses.settle(p->sim);
    const PeerStats s = tally(*p);
    std::printf("  peer %s: ticks=%u rollbacks=%u replayed=%u forced=%u resent=%u\n",
                role_of(cfg.sender), s.ticks, s.rollbacks, s.replayed, s.forced, s.resent);
    budget::report(role_of(cfg.sender), p->sim_cost, p->net_cost);
    // Замер обязан быть СВЕДЁН, а не только напечатан: проба, не сработавшая ни разу, печатает те
    // же нули, что и мгновенный кадр, и гейт 8 читал бы их как «влезли с запасом».
    if (!budget::swept(p->sim_cost, p->net_cost, s.ticks)) return 9;
    if (!peer_result::write_file(side_file(cfg, true, ".result"), observe(p->stage), s)) return 5;
    // Запись выкладывается ПОСЛЕ `settle` и рядом с отчётом: погашение отката переигрывает тики, то
    // есть переписывает хвост записи, и файл, выложенный до него, описывал бы предсказание. Гейт
    // ловит расхождение сверкой последнего хеша с маркой ЭТОГО отчёта — но только когда на выходе
    // откат действительно висит: в петле он гаснет раньше, и перестановка этих двух строк местами
    // проходит гейт зелёной. Порядок держится основанием, а не красным прогоном.
    return replay_record::write_run(side_file(cfg, true, ".replay"), p->sim) ? 0 : 7;
}

} // namespace platformer
