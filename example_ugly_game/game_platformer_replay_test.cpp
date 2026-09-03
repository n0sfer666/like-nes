#include <cstdio>
#include <string>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "hash.hpp"
#include "platform_args.hpp"
#include "platformer_replay_record.hpp"
#include "platformer_replay_run.hpp"

// Гейт 6 спеки #22 на НАСТОЯЩЕЙ сцене: записанный прогон переигрывается заново, и подделка в нём
// обязана быть названа НОМЕРОМ ТИКА, а не «поток не сошёлся».
//
// Игрушечная симуляция (`framework_replay_test`) этого не заменяет: там состояние — два числа, и
// заявка о нём есть само состояние. Здесь заявка — свёртка наблюдаемого сцены, и первый вопрос
// гейта ровно про неё: покрывает ли она то же, что называет `difference()`, или молчит про
// половину полей, которую живая игра читает после отката.
//
// Файл потока проверяет соседний гейт (`game_platformer_replay_io_test`): здесь поток живёт в
// памяти, и всё сказанное про него верно ещё до того, как у него появился формат.
namespace {

namespace ch = platformer::ch;
namespace replay = framework::replay;

using platformer::Mark;
using platformer::mark_hash;
using platformer::observe;
using platformer::replay_run::Stream;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

// Тик подделки взят В СЕРЕДИНЕ маршрута: на первом разошлось бы и переигрывание, начатое не с того
// состояния, а на последнем «расхождение в конце» неотличимо от накопленного за прогон.
constexpr uint32_t FORGED_AT = 200;

replay::Verdict replayed(const std::string& path, const Stream& s) {
    replay::Verdict v;
    check(platformer::replay_run::replay(path, s, v), "the level loads for the replay");
    return v;
}

// Заявка по позиции героя — сломанная реализация того же вопроса. Она обязана молчать там, где
// `difference()` называет поле: без неё утверждение «заявка остра» доказывает лишь то, что FNV
// меняется от смены байта.
uint64_t position_only(const Mark& m) {
    const int32_t xy[2] = {m.hero.position.x.raw, m.hero.position.y.raw};
    return asset::fnv1a(xy, sizeof(xy));
}

void test_the_claim_covers_what_the_word_names(const std::string& path) {
    platformer::Stage st;
    if (!platformer::load_stage(path, st)) return check(false, "the level loads for the claim");
    for (uint32_t t = 0; t < FORGED_AT; ++t) platformer::step_stage(st, platformer::script_input(t));

    const Mark m = observe(st);
    uint8_t buf[platformer::mark_io::BYTES];
    check(platformer::mark_bytes(m, buf) == platformer::mark_io::BYTES,
          "the claim is folded over the whole mark, not a prefix of it");

    // Четыре поля, которых нет в траектории и которые живая игра читает: окно прощения, запомненная
    // опора, счётчик работы физики и буфер событий.
    Mark window = m;
    window.hero.coyote_left += 1;
    Mark support = m;
    support.hero.support.index += 1;
    Mark work = m;
    work.world.counters.pairs += 1;
    Mark events = m;
    events.world.events_hash ^= 1ull;
    const Mark* bent[4] = {&window, &support, &work, &events};

    for (const Mark* b : bent) {
        check(platformer::difference(m, *b) != nullptr, "the word names this field");
        check(mark_hash(*b) != mark_hash(m), "and the claim moves with it");
        check(position_only(*b) == position_only(m),
              "control: a position-only claim is blind to every one of them");
    }
}

void test_an_honest_run_replays(const std::string& path, const Stream& s) {
    const replay::Verdict v = replayed(path, s);
    check(v.ok(), "the recorded run replays on a freshly loaded level");
    check(v.tick == s.ticks(), "and it replays to the very last tick");
}

void test_a_forged_input_is_named_by_tick(const std::string& path, const Stream& honest) {
    Stream s = honest;
    ch::MoveInput bent = platformer::script_input(FORGED_AT);
    bent.move_x = fix32::from_int(bent.move_x.raw > 0 ? -1 : 1);
    bent.jump_held = !bent.jump_held;
    check(s.forge_input(FORGED_AT, 0, bent), "the forgery lands in the stream");

    const replay::Verdict v = replayed(path, s);
    check(!v.ok(), "a stream whose input was bent is refused");
    if (v.tick != FORGED_AT) {
        std::printf("  FAIL: refused at tick %u, forged at %u\n", v.tick, FORGED_AT);
        ++fails;
    }
}

void test_a_forged_claim_is_named_by_tick(const std::string& path, const Stream& honest) {
    Stream s = honest;
    check(s.forge_claim(FORGED_AT, honest.claim(FORGED_AT) ^ 1ull), "the claim is bent by one bit");
    const replay::Verdict v = replayed(path, s);
    check(!v.ok() && v.tick == FORGED_AT, "a bent claim is refused at its own tick");
}

// Контроль источника: переигрывание обязано идти ТЕМ ЖЕ маршрутом, чей хеш прибит голденом. Разойдись
// они — все утверждения выше говорили бы про прогон, которого не проверяет никто.
void test_the_recorded_route_is_the_scripted_one(const std::string& path, const Stream& s) {
    platformer::Stage st;
    if (!platformer::load_stage(path, st)) return check(false, "the level loads for the route");
    const platformer::RunResult r = platformer::run_script(st, /*trace=*/false);
    check(s.ticks() == r.ticks, "the recording is as long as the scripted run");
    if (s.ticks() == 0) return check(false, "the recording has a last tick to compare");
    check(s.claim(s.ticks() - 1) == platformer::stage_claim(st),
          "and it ends where the scripted run ends");
}

// Запись прогона идёт ВНУТРИ тика, а инвариант 5 фреймворка запрещает там аллокацию: место под
// строки выписано `expect()` заранее. Утверждение об этом стоит здесь, потому что alloc-проба гейта
// отката ходит по `StageSim` — декоратор она не видит вовсе, и «аллокаций нет» было бы сказано про
// соседа. Контроль наоборот обязателен: счётчик, не видящий настоящей аллокации, показывает ноль на
// любом коде.
void test_recording_a_tick_touches_no_heap(const std::string& path) {
    platformer::Stage st;
    if (!platformer::load_stage(path, st)) return check(false, "the level loads for the recorder");
    platformer::replay_record::RecordingSim r;
    r.inner.stage = &st;
    const uint32_t n = platformer::script_ticks();
    r.expect(n);

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    for (uint32_t t = 0; t < n; ++t) {
        const ch::MoveInput in = platformer::script_input(t);
        r.step(&in);
    }
    const long during = framework::probe::allocs;
    framework::probe::in_hot = false;

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool got = framework::probe::control::plain_allocation();
    const long seen = framework::probe::allocs;
    framework::probe::in_hot = false;

    check(during == 0, "recording a tick touches the heap zero times");
    check(got && seen > 0, "control: the counter sees a real allocation");
    Stream s;
    check(platformer::replay_record::into_stream(r, s) && s.ticks() == n,
          "and the recorder hands over a stream as long as the run");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string path = DEFAULT_BUNDLE;
    for (int i = 1; i < argc; ++i) path = argv[i];

    std::printf("platformer sample: the scripted route as a verified replay\n");
    test_the_claim_covers_what_the_word_names(path);

    Stream honest;
    if (platformer::replay_run::record(path, honest)) {
        check(honest.ticks() == platformer::script_ticks(), "the recorder wrote a tick per step");
        test_the_recorded_route_is_the_scripted_one(path, honest);
        test_an_honest_run_replays(path, honest);
        test_a_forged_input_is_named_by_tick(path, honest);
        test_a_forged_claim_is_named_by_tick(path, honest);
        test_recording_a_tick_touches_no_heap(path);
        // Голден заявки: у `stage_claim` нет своего числа больше нигде, а Debug-этап спрашивает
        // ровно одно — совпадает ли оно на -O0 и на -O3. Цель, не печатающая литерала, стоит в его
        // списке молча и на этот вопрос не отвечает вовсе.
        if (honest.ticks() > 0)
            std::printf("  platformer replay claim = 0x%016llx\n",
                        static_cast<unsigned long long>(honest.claim(honest.ticks() - 1)));
    } else {
        check(false, "the level loads for the recording");
    }
    std::printf("game-platformer-replay: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
