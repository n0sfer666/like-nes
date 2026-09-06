#pragma once

#include "platformer_input_wire.hpp"
#include "platformer_peer_hooks.hpp"
#include "platformer_peer_step.hpp"
#include "platformer_sim.hpp"

// Очередь ввода одного пира: отдать СВОЙ тик и вычитать ЧУЖИЕ (спека #22, шаг C).
//
// Отдельно от прогона по той же границе, по которой раньше отделилась политика продвижения: там
// цикл и его исходы, здесь два конца провода. Гейт 9 добавил сюда шов живой половины — ввод стал
// приходить от того, кто вправе ответить «ещё не готов», — и пир, державший провод вместе с циклом,
// перевалил мягкий лимит длины.
namespace platformer {

// Отправка одного тика, которой гейт 9 умеет отказать. Настоящий отказ `Link::send` на петле не
// случается никогда — окно надёжного слоя (32) вчетверо шире потолка неподтверждённых
// (`PEER_INFLIGHT`), — поэтому ветка кэша без подставного отказа не прогонялась бы ни разу.
inline bool offer(Peer& p, const PeerConfig& cfg, uint32_t t, const uint8_t* body) {
    if (cfg.stall_send_tick >= 0 && static_cast<uint32_t>(cfg.stall_send_tick) == t &&
        !p.stalled_once) {
        p.stalled_once = true;
        return false;
    }
    return p.ch.link.send(body, input_wire::BYTES);
}

// `true` — тик уехал и очередь сдвинулась. Ответ нужен вызывающему: у живой половины долг по
// тикам копится не по одному (проход цикла с показом кадра длиннее тика в разы), и отдавать его
// приходится, пока шов отдаёт.
inline bool push_input(Peer& p, const PeerConfig& cfg, uint32_t total) {
    if (p.queued >= total || p.ch.link.unacked() >= PEER_INFLIGHT) return false;
    const uint32_t t = p.queued;
    // Ввод берётся у ЖИВОЙ половины, если она есть, и «ещё не готов» есть законный ответ: кадр
    // экрана длиннее прохода цикла в сотни раз, и отправитель без этого отказа сыграл бы весь
    // `PEER_INFLIGHT` за один кадр — то есть быстрее, чем человек успевает нажать.
    //
    // Взятый сэмпл ПЕРЕЖИВАЕТ отказ отправки: у скрипта повтор бесплатен, а у человека второй
    // вопрос на тот же тик вернул бы пустой ввод — фронты клавиш этого кадра съедены первым.
    ch::MoveInput in{};
    if (cfg.hooks == nullptr) {
        in = script_input(t);
    } else if (p.holding) {
        in = p.held;
    } else if (!cfg.hooks->input(t, in)) {
        return false;
    } else {
        p.held = in;
        p.holding = true;
    }
    const bool skip = cfg.drop_tick >= 0 && static_cast<uint32_t>(cfg.drop_tick) == t;
    if (!skip) {
        uint8_t body[input_wire::BYTES];
        if (!input_wire::put(body, t, in) || !offer(p, cfg, t, body)) {
            // Сломанная реализация контроля: сэмпл выбрасывается вместе с отказом отправки, и
            // следующий проход спрашивает шов о том же тике заново.
            if (cfg.forget_stalled) p.holding = false;
            return false;
        }
    }
    p.ses.deliver(t, 0, in);
    p.have[t] = 1;
    ++p.queued;
    p.holding = false;
    advance_known(p);
    return true;
}

// Исходы те же, что у `poll`, свёрнутые по всей очереди: `1` что-то приехало, `0` очередь пуста,
// `-2` сокет непригоден. Последний не сводится к первым двум: отказ шва и молчание собеседника
// лечатся по-разному, и пир, перепутавший их, дождался бы дедлайна вместо своего кода.
inline int drain(Peer& p) {
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

} // namespace platformer
