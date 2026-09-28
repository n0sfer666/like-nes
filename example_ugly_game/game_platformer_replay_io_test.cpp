#include <cstdio>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "platformer_replay_fixture.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "platformer_replay_run.hpp"

// Тот же гейт 6, но про ФАЙЛ: между двумя процессами поток ходит байтами, и всё, что верно про
// поток в памяти (`game_platformer_replay_test`), обязано пережить запись и чтение.
//
// Отдельной целью, а не ещё пятью утверждениями к соседу, по числу вопросов: тот спрашивает, остра
// ли заявка о состоянии, этот — тотален ли читатель. Расхождение в первом называется хешем, во
// втором — отказом разбора, и в одной цели имя первого терялось бы в формулировке второго.
namespace {

using platformer::replay_fixture::check;
using platformer::replay_fixture::rewrite;
using platformer::replay_fixture::write_head;

namespace replay = framework::replay;

using platformer::replay_run::Stream;

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

constexpr uint32_t FORGED_AT = 200;
constexpr size_t ROW = platformer::input_wire::BYTES;
constexpr size_t STRIDE = ROW + platformer::replay_io::CLAIM;

void test_the_file_carries_the_same_run(const std::string& path, const std::string& file,
                                        const Stream& honest) {
    check(platformer::replay_io::write_file(file, honest), "the stream writes to a file");
    Stream back;
    check(platformer::replay_io::read_file(file, back), "and reads back");
    check(back.ticks() == honest.ticks() && back.players() == honest.players(),
          "the file carries the width and the length of the run");
    bool same = back.ticks() == honest.ticks();
    for (replay::Tick t = 0; same && t < honest.ticks(); ++t)
        same = back.claim(t) == honest.claim(t) && back.row(t)[0] == honest.row(t)[0];
    check(same, "and every row and claim of it");

    replay::Verdict v;
    check(platformer::replay_run::replay(path, back, v), "the level loads for the replay");
    check(v.ok(), "the run read from the file replays");
}

// Подделка БАЙТОМ, а не через ручку потока: между процессами правят именно файл, и разбор обязан
// донести подделанный тик до верификатора, а не споткнуться о неё раньше.
void test_a_bent_claim_in_the_file_is_named(const std::string& path, const std::string& file,
                                            const std::vector<uint8_t>& bytes) {
    const size_t claim_at = platformer::replay_io::HEAD + FORGED_AT * STRIDE + ROW;
    if (claim_at >= bytes.size()) return check(false, "the forged tick is inside the file");

    std::vector<uint8_t> bent = bytes;
    bent[claim_at] ^= 0x40u;
    check(rewrite(file, bent), "the bent file is written back");
    Stream s;
    check(platformer::replay_io::read_file(file, s), "a bent claim is still a well-formed file");
    replay::Verdict v;
    check(platformer::replay_run::replay(path, s, v), "the level loads for the bent replay");
    check(!v.ok() && v.tick == FORGED_AT, "and the replay names the tick it was bent at");
}


} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::string path = DEFAULT_BUNDLE;
    for (int i = 1; i < argc; ++i) path = argv[i];

    std::printf("platformer sample: the recorded run as a file\n");
    Stream honest;
    if (platformer::replay_run::record(path, honest)) {
        const std::string file =
            platform::exe_path() + ".replay" + std::to_string(platform::process_id());
        test_the_file_carries_the_same_run(path, file, honest);

        std::vector<uint8_t> bytes;
        if (platform::read_bytes(file, bytes)) {
            test_a_bent_claim_in_the_file_is_named(path, file, bytes);
        } else {
            check(false, "the written file reads as bytes");
        }
        platform::remove_file(file);
    } else {
        check(false, "the level loads for the recording");
    }
    std::printf("game-platformer-replay-io: %s\n", platformer::replay_fixture::fails == 0 ? "PASS" : "FAIL");
    return platformer::replay_fixture::fails == 0 ? 0 : 1;
}
