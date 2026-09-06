#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "platform_process.hpp"
#include "platformer_net_hooks_fake.hpp"
#include "platformer_net_hooks_refusals.hpp"
#include "platformer_net_hooks_witness.hpp"
#include "platformer_net_runs.hpp"
#include "platformer_peer_argv.hpp"
#include "platformer_sim.hpp"

// Гейт 9 спеки #22, ШОВ: живая половина пира подаёт ввод и показывает кадр через `PeerHooks`, и
// прогон ЧЕРЕЗ шов обязан быть тем же прогоном, что и без него — та же марка и та же запись
// побайтно. Утверждение нужно потому, что окна на раннере нет: цель `game_platformer_net_live`
// здесь не запускается никогда, и без этого гейта первой проверкой шва за всю жизнь движка был бы
// ручной прогон владельца на двух машинах (§14 руководства).
//
// Здесь только УТВЕРЖДЕНИЯ: чем двигают пира — в `platformer_net_hooks_fake.hpp`. Всё, что можно
// проверить headless, проверяется здесь, а субъективную половину («играется, картинка не
// дёргается») закрывает владелец.
namespace {

using platformer::runs::Outcome;
using platformer::runs::report;
using platformer::runs::spawn_pair;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

// Прогон через шов обязан совпасть с прогоном без него И по марке, И по записи. Марок мало: они
// снимаются со сцены в конце, а запись ведётся тик за тиком — пара, чей ввод приехал не тем
// порядком, сходится маркой и расходится записью.
void test_the_seam_changes_nothing(const std::string& exe, const std::string& bundle,
                                   const std::string& prefix, const Outcome& plain) {
    Outcome o;
    const std::vector<std::string> hooked{"--hooks", "same"};
    if (!spawn_pair(exe, bundle, prefix + "-hooked", hooked, hooked, o)) {
        check(false, "both peers finished the run driven through the hooks");
        return;
    }
    report("hooked", o);
    const char* d = platformer::difference(o.send_mark, plain.send_mark);
    if (d != nullptr) {
        std::printf("  FAIL: the hooked run ended on a different %s than the plain one\n", d);
        ++fails;
    }
    check(o.send_replay == plain.send_replay && o.recv_replay == plain.recv_replay,
          "and both recorded runs are the same files, byte for byte");
    check(o.send.ticks == plain.send.ticks && o.recv.ticks == plain.recv.ticks,
          "and both sides played the same number of ticks");
}

// Позитивный контроль ДЛЯ утверждения выше: «через шов то же самое» верно и для `run_peer`,
// который шов не спрашивает вовсе. Отличает эти два случая только ввод, ОТЛИЧНЫЙ от скриптового:
// отправитель, стоящий на месте, обязан привести пару в ДРУГОЕ состояние.
void test_the_seam_is_really_asked(const std::string& exe, const std::string& bundle,
                                   const std::string& prefix, const Outcome& plain) {
    Outcome o;
    if (!spawn_pair(exe, bundle, prefix + "-idle", {"--hooks", "idle"}, {"--hooks", "same"}, o)) {
        check(false, "control: the pair whose sender stands still finished the run");
        return;
    }
    report("idle", o);
    check(platformer::difference(o.send_mark, plain.send_mark) != nullptr,
          "control: input given through the hooks changes where the run ends");
    // Вторая половина того же контроля: разойтись пара обязана с ЭТАЛОНОМ, а не друг с другом.
    // Иначе то же утверждение прошло бы на реализации, где ввод через шов приезжает только одному.
    check(platformer::difference(o.send_mark, o.recv_mark) == nullptr,
          "control: and the two peers still agree with each other");
}

// Живая половина вправе ответить «ещё не готов», и отвечает она так чаще, чем отдаёт ввод: кадр
// экрана длиннее прохода цикла. Прогон от этого обязан не измениться — отказ ОТКЛАДЫВАЕТ тик, а не
// теряет его. Заглушка, всегда готовая отдать ввод, эту ветку не отличает от отсутствующей.
void test_the_seam_may_say_not_yet(const std::string& exe, const std::string& bundle,
                                   const std::string& prefix, const Outcome& plain) {
    Outcome o;
    if (!spawn_pair(exe, bundle, prefix + "-slow", {"--hooks", "slow"}, {"--hooks", "same"}, o)) {
        check(false, "the pair whose sender is not always ready finished the run");
        return;
    }
    report("slow", o);
    check(platformer::difference(o.send_mark, plain.send_mark) == nullptr,
          "a seam that says \"not yet\" delays the tick instead of dropping it");
    check(o.send_replay == plain.send_replay && o.recv_replay == plain.recv_replay,
          "and the recorded run is still the same file, byte for byte");
    // Улика того, что отказывать шву было ЧЕМ: пара сходится с эталоном и тогда, когда «ещё не
    // готов» не прозвучало ни разу, — то есть на заглушке, тихо забывшей про режим, утверждение
    // выше проходит, ничего не проверив.
    platformer::fake::Witness w;
    check(platformer::fake::read_witness(prefix + "-slow", true, w),
          "the hooks witness of the not-always-ready sender was written");
    check(w.refused > 0, "and the seam really did say \"not yet\" at least once");
    platformer::fake::forget_witness(prefix + "-slow");
}

// Длину сессии называет ШОВ, и headless-умолчание тут не годится в ответе: пока заглушка отвечает
// тем же `script_ticks()`, тернарник в `run_peer` проверен ровно так же, как если бы его не было.
// Половина скрипта — число, которого у пира без хуков нет, и прогон обязан кончиться НА НЁМ.
void test_the_horizon_comes_from_the_seam(const std::string& exe, const std::string& bundle,
                                          const std::string& prefix, const Outcome& plain) {
    Outcome o;
    const std::vector<std::string> hooked{"--hooks", "short"};
    if (!spawn_pair(exe, bundle, prefix + "-short", hooked, hooked, o)) {
        check(false, "the pair playing half the script finished the run");
        return;
    }
    report("short", o);
    const uint32_t half = platformer::script_ticks() / 2;
    check(o.send.ticks == half && o.recv.ticks == half,
          "the session ends on the horizon named by the seam, not on the script's own length");
    check(platformer::difference(o.send_mark, plain.send_mark) != nullptr,
          "control: half a script really is a different run");
}

// Отказ отправки не теряет ВЗЯТОГО сэмпла: у скрипта повтор бесплатен, а человека второй раз о том
// же тике не спросишь — фронты клавиш кадра съедены первым чтением. `held` держит кэш и обязана
// совпасть с эталоном, `dropped` — та же пара со СЛОМАННЫМ кэшем, и она обязана разойтись: без
// второй половины «кэш работает» неотличимо от «отказа отправки не было». Отказ ПОДСТАВЛЯЕТСЯ:
// настоящий на петле не случается никогда, окно надёжного слоя вчетверо шире `PEER_INFLIGHT`.
void test_a_refused_send_keeps_the_sample(const std::string& exe, const std::string& bundle,
                                          const std::string& prefix, const Outcome& plain) {
    Outcome held;
    const std::vector<std::string> same{"--hooks", "same"};
    if (!spawn_pair(exe, bundle, prefix + "-held", {"--hooks", "held"}, same, held)) {
        check(false, "the pair whose sender was refused one send finished the run");
        return;
    }
    report("held", held);
    check(platformer::difference(held.send_mark, plain.send_mark) == nullptr,
          "a sample taken from the seam outlives the send that was refused");
    check(held.send_replay == plain.send_replay && held.recv_replay == plain.recv_replay,
          "and the recorded run is still the same file, byte for byte");
    Outcome dropped;
    if (!spawn_pair(exe, bundle, prefix + "-dropped", {"--hooks", "dropped"}, same, dropped)) {
        check(false, "control: the pair that forgets the refused sample finished the run");
        return;
    }
    report("dropped", dropped);
    // Сверяется ЗАПИСЬ, а не марка: запись ведётся тик за тиком, а марка снимается со сцены в
    // конце — потерянный тик разгона мог бы сойтись на ней случайно, и контроль был бы вакуумным.
    check(dropped.send_replay != plain.send_replay,
          "control: a sender that forgets that sample really does play a different run");
    platformer::fake::forget_witness(prefix + "-held");
    platformer::fake::forget_witness(prefix + "-dropped");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const char* mode = nullptr;
    std::vector<char*> rest;
    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--hooks") == 0 && i + 1 < argc) {
            mode = argv[++i];
            continue;
        }
        rest.push_back(argv[i]);
    }
    const int n = static_cast<int>(rest.size());
    if (platformer::is_peer_argv(n, rest.data())) {
        if (mode != nullptr) return platformer::fake::run_fake_peer(n, rest.data(), mode);
        platformer::PeerConfig cfg;
        if (!platformer::parse_peer(n, rest.data(), cfg)) return 3;
        return platformer::run_peer(cfg);
    }

    const std::string bundle = n >= 2 ? rest[1] : DEFAULT_BUNDLE;
    const std::string exe = platform::exe_path();
    const std::string prefix = exe + ".hooks" + std::to_string(platform::process_id());
    std::printf("platformer sample: the same run driven through the live half's seam\n");
    Outcome plain;
    if (exe.empty()) check(false, "the peer knows its own executable");
    if (fails == 0 && !spawn_pair(exe, bundle, prefix + "-plain", {}, {}, plain)) {
        check(false, "the reference pair without hooks finished the run");
    }
    if (fails == 0) {
        report("plain", plain);
        test_the_seam_changes_nothing(exe, bundle, prefix, plain);
        test_the_seam_is_really_asked(exe, bundle, prefix, plain);
        test_the_seam_may_say_not_yet(exe, bundle, prefix, plain);
        test_the_horizon_comes_from_the_seam(exe, bundle, prefix, plain);
        test_a_refused_send_keeps_the_sample(exe, bundle, prefix, plain);
        fails += platformer::hooks::refusals(exe, bundle, prefix);
    }
    std::printf("game-platformer-net-hooks: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
