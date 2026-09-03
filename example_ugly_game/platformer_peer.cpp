#include "platformer_peer.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "platformer_input_wire.hpp"
#include "platformer_peer_budget.hpp"
#include "platformer_peer_channel.hpp"
#include "platformer_peer_result.hpp"
#include "platformer_peer_step.hpp"
#include "platformer_replay_record.hpp"
#include "platformer_rollback.hpp"
#include "platformer_sim.hpp"

namespace platformer {
namespace {

// Сколько получатель ещё подтверждает после собственного конца. Выйди он сразу — последние
// подтверждения не дошли бы, и отправитель ждал бы их до дедлайна, объявив это отказом сети.
constexpr int64_t GRACE_MS = 1000;

const char* role_of(bool sender) { return sender ? "send" : "recv"; }

std::string side_file(const PeerConfig& c, bool mine, const char* ext) {
    return c.prefix + "-" + role_of(mine == c.sender) + ext;
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
    const channel::Where where{cfg.listen_port, cfg.peer_at};
    const channel::Meet met = channel::connect(p->ch, where, side_file(cfg, true, ".port"),
                                               side_file(cfg, false, ".port"), PEER_RENDEZVOUS_MS);
    if (met == channel::Meet::socket_bad) {
        std::printf("peer %s: port %u did not open\n", role_of(cfg.sender), cfg.listen_port);
        return 6;
    }
    if (met != channel::Meet::ok) return 3;

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
    // `aliens` печатается ТОЛЬКО здесь и в отчёт не кладётся: читает его человек в §14, где пир
    // запускается руками и его stdout виден. У гейтов он уходит в /dev/null, а формат результата
    // читают два бинаря — расширять его ради цифры, которую там никто не спросит, дороже.
    std::printf("  peer %s: ticks=%u rollbacks=%u replayed=%u forced=%u resent=%u aliens=%u\n",
                role_of(cfg.sender), s.ticks, s.rollbacks, s.replayed, s.forced, s.resent,
                p->ch.aliens);
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
