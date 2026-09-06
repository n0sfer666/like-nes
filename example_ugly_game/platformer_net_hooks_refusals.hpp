#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "platformer_net_hooks_witness.hpp"
#include "platformer_net_runs.hpp"

// Гейт 9 спеки #22, ОТКАЗЫ шва: три исхода, у которых прогон кончается ненулевым кодом, а не
// маркой. Отделены от утверждений о прогоне по той же границе, по которой оснастка отделена от
// гейта: там сверяются марки и записи ДОШЕДШИХ пар, здесь — коды и улики НЕДОШЕДШИХ. Счётчик
// проваленных возвращается числом: общего счётчика в репозитории нет, у каждого гейта он свой.
namespace platformer::hooks {

inline void refused_check(int& fails, bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

// Прогон пары, от которой ждут ОТКАЗА: `spawn_pair` требует нулей от обоих и о кодах говорит
// только строкой в лог.
inline bool both_peers_exit_with(const std::string& exe, const std::string& bundle,
                                 const std::string& prefix, const char* mode, int code) {
    platform::Child a;
    platform::Child b;
    const std::vector<std::string> hooks{"--hooks", mode};
    runs::forget(prefix);
    if (!a.spawn(runs::peer_argv(exe, "send", bundle, prefix, hooks))) return false;
    if (!b.spawn(runs::peer_argv(exe, "recv", bundle, prefix, hooks))) {
        a.kill_and_wait();
        return false;
    }
    platform::ExitStatus sa;
    platform::ExitStatus sb;
    const bool wa = a.wait(sa);
    const bool wb = b.wait(sb);
    std::printf("  %s: send code=%d, %s | recv code=%d, %s\n", mode, sa.code,
                runs::peer_said(sa, wa), sb.code, runs::peer_said(sb, wb));
    return wa && wb && sa.kind == platform::ExitKind::Exited && sa.code == code &&
           sb.kind == platform::ExitKind::Exited && sb.code == code;
}

// Потолок прогона называет шов, и он тоже обязан быть УСЛЫШАН: живая сессия втрое длиннее
// headless-дедлайна, и пир, оставивший себе прежние 20 секунд, отказал бы кодом 4 на третьей
// секунде игры. Миллисекунды не хватает никому — обе стороны обязаны уйти по дедлайну.
inline void a_deadline_comes_from_the_seam(int& fails, const std::string& exe,
                                           const std::string& bundle, const std::string& prefix) {
    refused_check(fails, both_peers_exit_with(exe, bundle, prefix, "impatient", 4),
                  "the deadline named by the seam is the one the peer gives up on");
    runs::forget(prefix);
    fake::forget_witness(prefix);
}

// Показ — вторая половина шва, и у неё свой отказ: человек закрыл окно. Заглушка, отвечающая
// `false`, обязана остановить прогон кодом 10, а не доиграть его до конца.
inline void show_can_stop_the_run(int& fails, const std::string& exe, const std::string& bundle,
                                  const std::string& prefix) {
    refused_check(fails, both_peers_exit_with(exe, bundle, prefix, "stop", 10),
                  "a peer whose window is gone stops by hand instead of playing on");
    // Улика РАННЕГО выхода, и она СЧИТАЕТ показанные кадры, а не смотрит на отсутствие файлов:
    // результат и запись пир пишет ПОСЛЕ цикла, то есть их нет и у реализации, доигравшей прогон
    // до конца и отказавшей на последнем кадре, — а отказ на первом кадре от отказа на последнем
    // отличает ровно один показ. Отсутствие файлов проверяется тут же: показ, остановивший цикл,
    // обязан унести с собой и запись.
    fake::Witness w;
    refused_check(fails, fake::read_witness(prefix, true, w),
                  "the hooks witness of the stopped peer was written");
    refused_check(fails, w.shown == 1,
                  "and it stopped on the FIRST frame it showed, not on the last one");
    refused_check(fails,
                  !platform::file_exists(prefix + "-send.result") &&
                      !platform::file_exists(prefix + "-send.replay"),
                  "and neither its result nor its recorded run were written");
    runs::forget(prefix);
    fake::forget_witness(prefix);
}

// Вырез флага строг НАРОЧНО: опечатка в режиме тихо давала бы прогон БЕЗ хуков — тот сошёлся бы с
// эталоном, и гейт остался бы зелёным, не спросив шов ни разу. Код у отказа СВОЙ (11): кодом 3
// отвечает и рандеву, не дождавшееся соседа, и утверждение проходило бы на заглушке, тихо
// принявшей незнакомый режим, — по чужой причине и восемью секундами позже.
inline void an_unknown_mode_is_refused(int& fails, const std::string& exe,
                                       const std::string& bundle, const std::string& prefix) {
    platform::Child a;
    runs::forget(prefix);
    if (!a.spawn(runs::peer_argv(exe, "send", bundle, prefix, {"--hooks", "bogus"}))) {
        refused_check(fails, false, "the peer under an unknown mode started at all");
        return;
    }
    platform::ExitStatus sa;
    const bool wa = a.wait(sa);
    std::printf("  bogus: send code=%d, %s\n", sa.code, runs::peer_said(sa, wa));
    refused_check(fails, wa && sa.kind == platform::ExitKind::Exited && sa.code == 11,
                  "an unknown --hooks mode is refused instead of quietly running without the seam");
    runs::forget(prefix);
}

inline int refusals(const std::string& exe, const std::string& bundle, const std::string& prefix) {
    int fails = 0;
    a_deadline_comes_from_the_seam(fails, exe, bundle, prefix + "-late");
    show_can_stop_the_run(fails, exe, bundle, prefix + "-stop");
    an_unknown_mode_is_refused(fails, exe, bundle, prefix + "-bogus");
    return fails;
}

} // namespace platformer::hooks
