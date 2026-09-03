#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "platformer_observed.hpp"
#include "platformer_peer_result.hpp"
#include "platformer_scene.hpp"
#include "platformer_sim.hpp"

// Оснастка гейтов 1 и 7 (спека #22, шаг C): как поднять пару пиров, собрать их отчёты и получить
// эталон одного процесса. Отдельно от самих утверждений, потому что предмет другой: здесь ЧЕМ
// меряют, там ЧТО утверждают, и в общем файле каждая правка запуска читалась бы как правка гейта.
namespace platformer::runs {

struct Outcome {
    bool ran = false;
    Mark send_mark;
    Mark recv_mark;
    PeerStats send;
    PeerStats recv;
};

inline std::vector<std::string> peer_argv(const std::string& exe, const char* role,
                                   const std::string& bundle, const std::string& prefix,
                                   const std::vector<std::string>& extra) {
    std::vector<std::string> argv{exe, "--peer", role, bundle, prefix};
    argv.insert(argv.end(), extra.begin(), extra.end());
    return argv;
}

inline void forget(const std::string& prefix) {
    for (const char* side : {"-send", "-recv"})
        for (const char* ext : {".port", ".result"}) platform::remove_file(prefix + side + ext);
}

// Расшифровка исхода ребёнка. Нужна потому, что stdout пира уходит в никуда (`Child::spawn` отдаёт
// его в /dev/null и NUL), то есть ни одна его строка до лога не доезжает: без перевода в логе стоял
// бы «peers exited send=6», и «сокет непригоден» было бы неотличимо от «не дождались исхода» —
// оба дают -1 в поле кода.
inline const char* peer_said(const platform::ExitStatus& s, bool waited) {
    if (!waited) return "no exit status at all (the seam stopped waiting)";
    if (s.kind == platform::ExitKind::Crashed) return "crashed";
    if (s.kind == platform::ExitKind::Killed) return "killed";
    if (s.kind != platform::ExitKind::Exited) return "unknown exit";
    switch (s.code) {
        case 0: return "ok";
        case 2: return "the level did not load";
        case 3: return "no port from the other peer";
        case 4: return "gave up at the deadline";
        case 5: return "could not write its result";
        case 6: return "the socket went bad";
        default: return "an unexpected code";
    }
}

// Оба ребёнка запускаются ДО первого ожидания: `wait` блокирует, и запуск второго после него был бы
// не двумя процессами, а очередью из одного — первый ждал бы порт соседа, которого ещё нет, до
// собственного дедлайна. Висеть тут нечему: у пира свой потолок и на рандеву, и на весь прогон,
// и оба уложены под конечное ожидание шва (`PEER_RENDEZVOUS_MS`, `PEER_DEADLINE_MS`).
inline bool spawn_pair(const std::string& exe, const std::string& bundle, const std::string& prefix,
                const std::vector<std::string>& send_extra,
                const std::vector<std::string>& recv_extra, Outcome& out) {
    forget(prefix);
    platform::Child a;
    platform::Child b;
    if (!a.spawn(peer_argv(exe, "send", bundle, prefix, send_extra))) return false;
    if (!b.spawn(peer_argv(exe, "recv", bundle, prefix, recv_extra))) {
        a.kill_and_wait();
        forget(prefix);
        return false;
    }
    platform::ExitStatus sa;
    platform::ExitStatus sb;
    // Ждут ОБОИХ безусловно: короткое замыкание `&&` означало бы, что на отказе первого второго не
    // ждали вовсе, и здорового получателя убивает деструктор из-за отправителя — в логе это
    // «recv=-1», хотя его никто не спрашивал.
    const bool wa = a.wait(sa);
    const bool wb = b.wait(sb);
    if (!wa || !wb || sa.kind != platform::ExitKind::Exited || sa.code != 0 ||
        sb.kind != platform::ExitKind::Exited || sb.code != 0) {
        std::printf("  peer send: code=%d, %s\n  peer recv: code=%d, %s\n", sa.code,
                    peer_said(sa, wa), sb.code, peer_said(sb, wb));
        forget(prefix);
        return false;
    }
    out.ran = peer_result::read_file(prefix + "-send.result", out.send_mark, out.send) &&
              peer_result::read_file(prefix + "-recv.result", out.recv_mark, out.recv);
    // Файлы сносятся только после УДАВШЕГОСЯ чтения: прочитать их не вышло — значит смотреть в них
    // будет человек, и уборка отобрала бы у него единственную улику.
    if (out.ran) forget(prefix);
    return out.ran;
}

// Эталон одного процесса: тот же уровень, тот же скрипт, никакой сети. Без него «пиры сошлись»
// верно и для двух процессов, разошедшихся с одиночной игрой ОДИНАКОВО.
inline bool single_process_mark(const std::string& bundle, Mark& out) {
    Stage st;
    if (!load_stage(bundle, st)) return false;
    const uint32_t n = script_ticks();
    for (uint32_t t = 0; t < n; ++t) step_stage(st, script_input(t));
    out = observe(st);
    return true;
}

inline void report(const char* run, const Outcome& o) {
    std::printf("  %s: send ticks=%u rollbacks=%u forced=%u resent=%u | recv ticks=%u "
                "rollbacks=%u forced=%u\n",
                run, o.send.ticks, o.send.rollbacks, o.send.forced, o.send.resent, o.recv.ticks,
                o.recv.rollbacks, o.recv.forced);
}

} // namespace platformer::runs
