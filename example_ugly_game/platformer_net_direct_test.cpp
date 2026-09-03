#include <cstdio>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_net.hpp"
#include "platform_process.hpp"
#include "platformer_net_ports.hpp"
#include "platformer_net_runs.hpp"
#include "platformer_peer.hpp"
#include "platformer_peer_argv.hpp"

// Гейт 9 спеки #22, половина его цены: пара пиров сходится, назвав адреса друг друга АРГУМЕНТАМИ,
// без единого общего файла. Вторую половину — реальный лаг между двумя машинами — закрывает
// владелец руками (§14 руководства): раннер обеих машин не имеет, и сеть у него одна на процесс.
//
// Отдельной целью от гейта петли, потому что предмет другой: тот утверждает, что из одного ввода
// два адресных пространства приходят в одно состояние, этот — что сосед может быть НАЗВАН. До
// этого раунда назвать его было нечем: знакомство умело только эфемерный порт петли, опубликованный
// в файл, то есть требовало общей файловой системы — а её у двух машин нет по определению.
namespace {

using platformer::ports::at_loopback;
using platformer::runs::Outcome;
using platformer::runs::peer_argv;
using platformer::runs::peer_said;
using platformer::runs::report;
using platformer::runs::single_process_mark;
using platformer::runs::spawn_pair;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

void test_named_peers_meet_without_a_shared_file(const std::string& exe, const std::string& bundle,
                                                 const std::string& prefix,
                                                 const platformer::Mark& alone) {
    uint16_t send_port = 0;
    uint16_t recv_port = 0;
    if (!platformer::ports::pick_two(send_port, recv_port)) {
        check(false, "two free ports were found for the pair");
        return;
    }
    Outcome o;
    const std::vector<std::string> send_extra{"--listen", std::to_string(send_port), "--at",
                                              at_loopback(recv_port)};
    const std::vector<std::string> recv_extra{"--listen", std::to_string(recv_port), "--at",
                                              at_loopback(send_port)};
    if (!spawn_pair(exe, bundle, prefix + "-direct", send_extra, recv_extra, o)) {
        check(false, "both peers finished the run addressed by hand");
        return;
    }
    report("direct", o);
    const char* d = platformer::difference(o.send_mark, o.recv_mark);
    if (d != nullptr) {
        std::printf("  FAIL: the two named peers ended on a different %s\n", d);
        ++fails;
    }
    const char* s = platformer::difference(o.send_mark, alone);
    if (s != nullptr) {
        std::printf("  FAIL: the named pair ended on a different %s than the single process\n", s);
        ++fails;
    }
    check(o.send.ticks == platformer::script_ticks() && o.recv.ticks == platformer::script_ticks(),
          "both named peers played the whole script");
    // Ровно то, что §14 руководства велит владельцу сверить руками двумя sha256 на двух машинах.
    // Здесь оно стоит утверждением, потому что иначе первой проверкой этого равенства за всю жизнь
    // движка был бы ручной прогон владельца — а марки сходятся и у пары, чьи ЗАПИСИ разъехались:
    // марка снимается со сцены в конце, запись ведётся тик за тиком.
    check(!o.send_replay.empty() && o.send_replay == o.recv_replay,
          "and the two recorded runs are the same file, byte for byte");
    // Контроль ПУТИ, без которого утверждение выше верно и для пары, тихо ушедшей знакомиться
    // файлом: марки сошлись бы, скрипт доигрался бы, а про адресацию гейт не сказал бы ничего.
    check(!o.met_by_file, "control: and they never published a port file to find each other");
}

// Позитивный контроль ДЛЯ КОНТРОЛЯ выше. `met_by_file` — единственная улика прямой адресации, и
// утверждение «файла нет» зелено само по себе: переименуй файл рандеву, положи его в другой
// каталог, снеси проверку существования — и гейт продолжит молчать, ничего не проверяя. Поэтому
// рядом стоит пара БЕЗ адресов: у неё тот же вопрос обязан ответить «файл был».
void test_the_file_rendezvous_is_visible_when_it_happens(const std::string& exe,
                                                         const std::string& bundle,
                                                         const std::string& prefix) {
    Outcome o;
    if (!spawn_pair(exe, bundle, prefix + "-byfile", {}, {}, o)) {
        check(false, "the pair without addresses finished the run");
        return;
    }
    report("by file", o);
    check(o.met_by_file, "control: a pair given no addresses does publish its port files");
}

// Половина адресации — отказ, а не откат к файлам. Проверяется ЗАПУСКОМ, потому что утверждение
// ровно про поведение процесса: пир, у которого названа половина, обязан выйти кодом 3 («сосед не
// назван»), а не дойти до конца, познакомившись по-старому.
//
// Половины ДВЕ, и обе спрашиваются. Утверждай гейт только «свой порт без соседа» — вырезанная из
// `connect` вторая половина условия осталась бы неназванной: пир с одним `--at` привязался бы к
// эфемерному порту, доиграл бы до дедлайна и отдал бы код 4, которого никто не спрашивал.
void test_half_an_address_is_refused(const std::string& exe, const std::string& bundle,
                                     const std::string& prefix, const char* half,
                                     const std::vector<std::string>& extra) {
    platform::Child c;
    const std::string mine = prefix + "-half-" + half;
    if (!c.spawn(peer_argv(exe, "send", bundle, mine, extra))) {
        check(false, "the half-addressed peer started");
        return;
    }
    platform::ExitStatus st;
    const bool waited = c.wait(st);
    std::printf("  half-addressed (%s): code=%d, %s\n", half, st.code, peer_said(st, waited));
    check(waited && st.kind == platform::ExitKind::Exited && st.code == 3,
          "a peer given half an address refuses instead of running");
    // Одного кода мало: тот же код 3 отдаёт и пир, который УШЁЛ знакомиться файлом и не дождался
    // соседа — соседа тут нет вовсе. Различает их улика: откатившийся сначала опубликовал бы
    // СВОЙ порт файлом. Без этой строки утверждение выше зелено и на реализации, где отбитие
    // половинчатой адресации вырезано целиком.
    check(!platform::file_exists(mine + "-send.port"),
          "control: and it refused by its arguments, not by waiting for a file that never comes");
    platformer::runs::forget(mine);
}

// Занятый номер порта — САМАЯ вероятная ошибка владельца в двухмашинном прогоне, и отвечать на неё
// кодом 3 значило бы отправить его искать опечатку в `--at`, которой нет. Утверждение держит
// разведение исходов `connect`: сведи их обратно в один `false`, и руководство (§14, шаг 1) начнёт
// называть владельцу не ту причину — а прогон останется зелёным, потому что пир всё равно вышел.
void test_a_taken_port_is_named_as_such(const std::string& exe, const std::string& bundle,
                                        const std::string& prefix) {
    uint16_t port = 0;
    if (!platformer::ports::pick_one(port)) {
        check(false, "a free port was found to occupy");
        return;
    }
    platform::net::Socket taken;
    if (!taken.open(platform::net::ADDRESS_LOOPBACK, port)) {
        check(false, "the port could be occupied for the test");
        return;
    }
    platform::Child c;
    const std::string mine = prefix + "-busy";
    if (!c.spawn(peer_argv(exe, "send", bundle, mine,
                           {"--listen", std::to_string(port), "--at",
                            at_loopback(static_cast<uint16_t>(port + 1))}))) {
        check(false, "the peer aiming at a taken port started");
        return;
    }
    platform::ExitStatus st;
    const bool waited = c.wait(st);
    std::printf("  taken port: code=%d, %s\n", st.code, peer_said(st, waited));
    check(waited && st.kind == platform::ExitKind::Exited && st.code == 6,
          "a peer whose port is already taken says the socket went bad, not that nobody was named");
    platformer::runs::forget(mine);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    // Непонятый аргумент — тот же код 3, что и «сосед не назван»: с точки зрения прогона это одно
    // и то же событие — пир не знает, с кем и откуда говорить, — а различает их напечатанное слово.
    if (platformer::is_peer_argv(argc, argv)) {
        platformer::PeerConfig cfg;
        if (!platformer::parse_peer(argc, argv, cfg)) return 3;
        return platformer::run_peer(cfg);
    }

    const std::string bundle = argc >= 2 ? argv[1] : DEFAULT_BUNDLE;
    const std::string exe = platform::exe_path();
    const std::string prefix = exe + ".direct" + std::to_string(platform::process_id());
    std::printf("platformer sample: two processes that name each other\n");
    platformer::Mark alone;
    if (exe.empty()) check(false, "the peer knows its own executable");
    if (!single_process_mark(bundle, alone)) check(false, "the single-process reference ran");
    if (fails == 0) {
        test_named_peers_meet_without_a_shared_file(exe, bundle, prefix, alone);
        test_the_file_rendezvous_is_visible_when_it_happens(exe, bundle, prefix);
        uint16_t lone = 0;
        if (!platformer::ports::pick_one(lone)) {
            check(false, "a free port was found for the half-addressed peer");
        } else {
            test_half_an_address_is_refused(exe, bundle, prefix, "listen",
                                            {"--listen", std::to_string(lone)});
            test_half_an_address_is_refused(exe, bundle, prefix, "at",
                                            {"--at", at_loopback(lone)});
        }
        test_a_taken_port_is_named_as_such(exe, bundle, prefix);
    }
    std::printf("game-platformer-net-direct: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
