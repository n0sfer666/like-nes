#include <cstdio>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "platformer_net_runs.hpp"
#include "platformer_peer.hpp"
#include "platformer_peer_argv.hpp"
#include "platformer_replay_run.hpp"

// Гейты 1 и 7 спеки #22: ДВА ПРОЦЕССА на одном вводе приходят в одно состояние, и пир, оглохший
// посреди прогона, догоняет и сходится.
//
// Двоичный файл один, а ролей три: без аргументов — распорядитель, `--peer send|recv …` — сам пир.
// Так требует шов процессов (`platform_process.hpp`): fork'а на Windows нет, и единственный
// переносимый способ получить второй процесс — перезапустить СОБСТВЕННЫЙ exe со служебным флагом.
//
// Почему процессы, а не потоки: гейт утверждает, что состояние симуляции целиком лежит там, где мы
// думаем. Два потока делят кучу, статику и одно и то же время — то есть ровно те места, где
// скрытое состояние и прячется, — и молча сошлись бы на нём.
namespace {

using platformer::Mark;
using platformer::runs::Outcome;
using platformer::runs::report;
using platformer::runs::single_process_mark;
using platformer::runs::spawn_pair;

int fails = 0;

// Возвращает свой же ответ: утверждения о ЗАПИСИ идут цепочкой (разобралось → переиграло →
// сошлось), и продолжать её после первого провала значит читать поток, которого нет.
bool check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
    return ok;
}

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

// Гейт 6, шаг C: прогон, сыгранный ЧУЖИМ процессом, проверяется здесь целиком — тик за тиком, из
// одного лишь записанного файла и уровня. Эталона состояния у проверяющего нет: он его пересчитывает.
//
// Последняя заявка обязана совпасть с маркой ИЗ ОТЧЁТА этого же процесса. Без этого «поток
// верифицировался» верно и для записи, которую пир сделал не про свой прогон, — например, записав
// отправленный ввод вместо сыгранного.
bool recorded_run_verifies(const std::string& bundle, const std::vector<uint8_t>& bytes,
                           const Mark& mark, platformer::replay_io::Stream& s) {
    if (!check(platformer::replay_io::parse(bytes, s), "the recorded run of a peer parses"))
        return false;
    if (!check(s.ticks() == platformer::script_ticks(), "and it is as long as the script"))
        return false;
    framework::replay::Verdict v;
    if (!check(platformer::replay_run::replay(bundle, s, v), "a fresh level loads to replay it"))
        return false;
    check(v.ok(), "the run of the other process verifies tick by tick");
    return check(s.claim(s.ticks() - 1) == platformer::mark_hash(mark),
                 "and ends on the very state that process reported");
}

void test_two_processes_on_one_input_agree(const std::string& exe, const std::string& bundle,
                                           const std::string& prefix, const Mark& alone) {
    Outcome o;
    if (!spawn_pair(exe, bundle, prefix + "-pair", {}, {}, o)) {
        check(false, "both peers finished the run");
        return;
    }
    report("lockstep", o);
    const uint32_t n = platformer::script_ticks();
    const char* d = platformer::difference(o.send_mark, o.recv_mark);
    if (d != nullptr) {
        std::printf("  FAIL: the two processes ended on a different %s\n", d);
        ++fails;
    }
    const char* s = platformer::difference(o.send_mark, alone);
    if (s != nullptr) {
        std::printf("  FAIL: the pair ended on a different %s than the single process\n", s);
        ++fails;
    }
    check(o.send.ticks == n && o.recv.ticks == n, "both peers played the whole script");
    // Порог здесь СТРУКТУРНЫЙ, а не измеренный: число откатов получателя задаёт загруженность
    // машины (семь прогонов на простаивающей дали 12..19, а соседний прогон с пропущенным кадром —
    // 4), и любая цифра выше нуля краснела бы на здоровом прогоне под нагрузкой раннера. Ноль же
    // означал бы, что механизм отката не работал вовсе и «пиры сошлись» сказано про прогон, где
    // сходиться было нечему.
    check(o.send.rollbacks > 0 && o.recv.rollbacks > 0, "control: both peers really did roll back");
    check(o.send.forced == 0 && o.recv.forced == 0, "neither peer gave up waiting for input");
    check(o.send.conflicts == 0 && o.recv.conflicts == 0, "no tick got two different inputs");
    check(o.send.too_deep == 0 && o.recv.too_deep == 0, "nothing arrived past the rollback depth");
    check(o.send.too_far == 0 && o.recv.too_far == 0, "nothing arrived past the input lead");
}

// Сломанная реализация, которой гейт 1 требует поимённо: один `InputFrame` не уходит в сеть вовсе.
// Получатель играет тот тик предсказанием и не узнаёт правду никогда — хеши обязаны РАЗОЙТИСЬ.
void test_a_skipped_input_frame_is_caught(const std::string& exe, const std::string& bundle,
                                          const std::string& prefix) {
    Outcome o;
    const uint32_t at = platformer::script_ticks() / 3;
    if (!spawn_pair(exe, bundle, prefix + "-drop", {"--drop", std::to_string(at)}, {}, o)) {
        check(false, "both peers finished the run with a dropped frame");
        return;
    }
    report("dropped", o);
    check(platformer::difference(o.send_mark, o.recv_mark) != nullptr,
          "a skipped input frame leaves the two processes in different states");
    // Ровно один форсированный тик и полный скрипт, а не «хоть что-то разошлось»: утверждение
    // `марки разошлись + forced > 0` верно и для прогона, оборвавшегося на десятой дыре по причине,
    // к пропущенному кадру отношения не имеющей. Дыра здесь ОДНА по построению.
    check(o.recv.ticks == platformer::script_ticks(), "and it played the script to the end anyway");
    check(o.recv.forced == 1, "and the receiver says why: it stopped waiting for that one tick");

    // Гейт ОСНОВАНИЮ записи: пир пишет то, что СЫГРАЛ, а не то, что отправил. Разошлись эти два
    // ровно здесь — дыру получатель доиграл предсказанием, — и его поток обязан верифицироваться
    // САМ ПО СЕБЕ, кончаясь на его собственной марке, которая с маркой отправителя не совпадает
    // (утверждение выше). Записывай пир отправленное — поток был бы потоком отправителя, и
    // последняя заявка не сошлась бы с его отчётом. В честном прогоне это неотличимо: там оба
    // потока одинаковы по построению, и гейт был бы зелен на обеих реализациях.
    platformer::replay_io::Stream recv;
    platformer::replay_io::Stream send;
    if (!recorded_run_verifies(bundle, o.recv_replay, o.recv_mark, recv)) return;
    if (!recorded_run_verifies(bundle, o.send_replay, o.send_mark, send)) return;
    // Утверждается РАСХОЖДЕНИЕ и его нижняя граница, а не номер тика. Точный номер здесь не
    // инвариант, и прогон это показал: дыра на 139 сыграна предсказанием, которое СОВПАЛО по
    // значению — тик 139 лежит внутри полосы скрипта `[134,142)`, где ввод не меняется, — а первое
    // расхождение всплыло на 171, границе следующей полосы. Дальше их 169 при `too_far=257`:
    // пока получатель ждал дыру, подтверждения отправителя уезжали за `LEAD` от его стоящей головы
    // и отбрасывались, а форсирование объявило те тики известными навсегда. Сколько их — считает
    // планировщик машины, поэтому равенству тут взяться неоткуда.
    //
    // Нижняя граница СТРУКТУРНА: до пропавшего кадра весь ввод получателя подтверждён, расходиться
    // нечему. И сломанную реализацию она ловит ту же — пиши пир отправленное вместо сыгранного,
    // потоки совпали бы целиком и расхождения не нашлось бы вовсе.
    uint32_t first = platformer::script_ticks();
    for (uint32_t t = 0; t < send.ticks() && first == platformer::script_ticks(); ++t)
        if (!(recv.row(t)[0] == send.row(t)[0])) first = t;
    check(first < send.ticks() && first >= at,
          "and the recording of the receiver differs from the sender's, never before the gap");
}

// Гейт 7: получатель перестаёт читать сокет посреди прогона. Отправитель упирается в потолок
// неподтверждённых и ждёт, надёжный слой переотправляет — и после возврата слуха пир догоняет.
void test_a_deaf_peer_catches_up(const std::string& exe, const std::string& bundle,
                                 const std::string& prefix, const Mark& alone) {
    Outcome o;
    const uint32_t at = platformer::script_ticks() / 2;
    if (!spawn_pair(exe, bundle, prefix + "-deaf", {}, {"--deaf", std::to_string(at), "500"}, o)) {
        check(false, "both peers finished the run with a deaf stretch");
        return;
    }
    report("deafened", o);
    const char* d = platformer::difference(o.recv_mark, alone);
    if (d != nullptr) {
        std::printf("  FAIL: the deafened peer converged on a different %s\n", d);
        ++fails;
    }
    check(o.recv.ticks == platformer::script_ticks(), "the deafened peer played the whole script");
    check(o.recv.forced == 0, "it caught up by waiting, not by giving up");
    // Без этих трёх «сошлись по хешу» верно и для прогона, в котором глухоты не случилось вовсе:
    // окно, не открывшееся или открывшееся на финишной черте, не стоит ничего.
    check(o.recv.deaf_from >= at, "control: the deaf window opened where it was asked to");
    // Обе границы, а не одна: `deaf_to` по умолчанию ноль, поэтому «меньше конца скрипта» верно и
    // для окна, которое НЕ ЗАКРЫЛОСЬ вовсе. Закрытие доказывает вторая половина — у открывшегося
    // окна `deaf_to` не может быть меньше `deaf_from`, а у неоткрывшегося он ноль при `deaf_from`
    // не меньше `at`. Требовать РОСТА тика внутри окна нельзя: пир, у которого запас кончился,
    // честно стоит на месте, и это утверждает счётчик простоя ниже.
    check(o.recv.deaf_to >= o.recv.deaf_from && o.recv.deaf_to < platformer::script_ticks(),
          "control: and it closed mid-script, not on the finish line");
    check(o.recv.deaf_stalls > 0,
          "control: the silence really did stop it: no input, no step");
    check(o.send.resent > 0, "control: and the sender had to say it all again");
}

void test_the_recorded_run_of_a_peer_verifies(const std::string& exe, const std::string& bundle,
                                              const std::string& prefix) {
    Outcome o;
    if (!spawn_pair(exe, bundle, prefix + "-replay", {}, {}, o)) {
        check(false, "both peers finished the run and wrote it down");
        return;
    }
    report("recorded", o);
    platformer::replay_io::Stream s;
    if (!recorded_run_verifies(bundle, o.send_replay, o.send_mark, s)) return;

    // Подделка БАЙТОМ в приехавшем файле: между процессами едет он, и правит его тот, кто хочет
    // выдать чужой прогон за свой. Отказ обязан назвать НОМЕР ТИКА — иначе «файл не сошёлся»
    // сказано про четыреста двадцать тиков разом.
    const size_t row = platformer::input_wire::BYTES;
    const uint32_t at = platformer::script_ticks() / 3;
    const size_t claim_at = platformer::replay_io::HEAD + at * (row + platformer::replay_io::CLAIM);
    if (!check(claim_at + row < o.send_replay.size(), "the forged tick is inside the file")) return;
    std::vector<uint8_t> bent = o.send_replay;
    bent[claim_at + row] ^= 0x40u;
    platformer::replay_io::Stream b;
    framework::replay::Verdict v;
    check(platformer::replay_io::parse(bent, b), "a bent claim is still a well-formed file");
    check(platformer::replay_run::replay(bundle, b, v) && !v.ok() && v.tick == at,
          "and the verifier names the tick it was bent at");
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
    // В имя файлов рандеву входит pid распорядителя: два прогона одного двоичного файла рядом
    // (ctest -j, две ветки на одном раннере) иначе читали бы порт друг друга и сходились бы
    // крест-накрест — то есть гейт краснел бы от соседа, а не от дефекта.
    const std::string prefix = exe + ".net" + std::to_string(platform::process_id());
    std::printf("platformer sample: two processes over the loopback\n");
    Mark alone;
    if (exe.empty()) check(false, "the peer knows its own executable");
    if (!single_process_mark(bundle, alone)) check(false, "the single-process reference ran");
    if (fails == 0) {
        test_two_processes_on_one_input_agree(exe, bundle, prefix, alone);
        test_a_skipped_input_frame_is_caught(exe, bundle, prefix);
        test_a_deaf_peer_catches_up(exe, bundle, prefix, alone);
        test_the_recorded_run_of_a_peer_verifies(exe, bundle, prefix);
    }
    std::printf("game-platformer-net: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
