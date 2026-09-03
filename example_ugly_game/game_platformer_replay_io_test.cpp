#include <cstdio>
#include <string>
#include <vector>

#include "platform_args.hpp"
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

namespace replay = framework::replay;

using platformer::replay_run::Stream;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

const char* DEFAULT_BUNDLE = "example_ugly_game/assets/game.bundle";

constexpr uint32_t FORGED_AT = 200;
constexpr size_t ROW = platformer::input_wire::BYTES;
constexpr size_t STRIDE = ROW + platformer::replay_io::CLAIM;

bool rewrite(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::FILE* f = platform::open_file(path, "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    return std::fclose(f) == 0 && ok;
}

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

// Перестановка строк местами не трогает ни длины файла, ни его хвоста: без номера тика ВНУТРИ
// строки читатель принял бы её за честный поток, разошедшийся на первом же тике.
void test_two_rows_swapped_is_a_refusal(const std::string& file,
                                        const std::vector<uint8_t>& bytes) {
    std::vector<uint8_t> swapped = bytes;
    const size_t a = platformer::replay_io::HEAD;
    const size_t b = a + STRIDE;
    for (size_t i = 0; i < ROW; ++i) std::swap(swapped[a + i], swapped[b + i]);
    check(rewrite(file, swapped), "the swapped file is written back");
    Stream s;
    check(!platformer::replay_io::read_file(file, s),
          "two rows swapped is a refusal, not a run that diverges at tick zero");
}

// «Дочитали до конца» обязано быть отказом, а не прогоном, кончившимся раньше: файл мог не
// дописаться — ребёнка убили, диск кончился, — и короткий поток верифицируется зелёным.
//
// Оба обреза — и по границе записи, и посреди неё — валит ОДНА строка: сверка длины с заголовком.
// Написать здесь «полтика отбивает сам читатель» было бы враньём про механизм: до читателя такой
// файл не доезжает вовсе. Случаи всё равно два, потому что вопросы разные — целое число тиков без
// сверки читалось бы как честный прогон покороче, а полтика уронило бы разбор посреди поля.
void test_a_truncated_file_is_refused(const std::string& file,
                                      const std::vector<uint8_t>& bytes) {
    std::vector<uint8_t> whole_tick(bytes.begin(), bytes.end() - STRIDE);
    check(rewrite(file, whole_tick), "the file cut by a whole tick is written back");
    Stream short_run;
    check(!platformer::replay_io::read_file(file, short_run),
          "a file a tick shorter than its header says is refused, not read as a shorter run");

    std::vector<uint8_t> cut(bytes.begin(), bytes.end() - 5);
    check(rewrite(file, cut), "the truncated file is written back");
    Stream s;
    check(!platformer::replay_io::read_file(file, s),
          "a file cut in the middle of a record is refused too");

    // Контроль наоборот: отказ выше обязан быть про ДЛИНУ, а не про то, что читателю не нравится
    // любой переписанный файл. Те же байты целиком — и он их принимает.
    check(rewrite(file, bytes), "the whole file is written back");
    Stream whole;
    check(platformer::replay_io::read_file(file, whole),
          "control: the same reader accepts the untouched bytes");
}

// Подпись формата: файл чужого вида обязан быть отбит подписью, а не разобран как поток, у которого
// первые четыре байта случайно сошлись за номер тика.
void test_a_foreign_file_is_refused(const std::string& file, const std::vector<uint8_t>& bytes) {
    std::vector<uint8_t> foreign = bytes;
    foreign[0] = 'X';
    check(rewrite(file, foreign), "the foreign file is written back");
    Stream s;
    check(!platformer::replay_io::read_file(file, s), "a file that is not a replay is refused");
}

// Дописанный хвост: заголовок честен, все тики на месте, но за последним из них лежат ещё байты.
// Читатель, дочитавший «свои» записи и не спросивший про остаток, принял бы такой файл — то есть
// принял бы и склейку двух прогонов, и дописанное кем-то продолжение.
void test_a_file_with_a_tail_is_refused(const std::string& file,
                                        const std::vector<uint8_t>& bytes) {
    std::vector<uint8_t> longer = bytes;
    longer.insert(longer.end(), 5, 0u);
    check(rewrite(file, longer), "the file with a tail is written back");
    Stream s;
    check(!platformer::replay_io::read_file(file, s),
          "bytes past the last tick are a refusal, not a run that read fine");
}

// Заголовок, сочиняющий себе размеры: файл в двенадцать байт заявляет миллион игроков и миллион
// тиков. Отказ обязан случиться ДО того, как по этим числам что-нибудь выпишут.
//
// Вторая пара чисел подобрана так, чтобы ПЕРЕПОЛНИТЬ подсчёт длины: 954437176 · 9 + 8, умноженное
// на 2^31, даёт в uint64 ровно двенадцать — то есть сверка умножением сходится сама с собой, файл в
// один заголовок проходит её и заставляет выписать под строку 7.6 ГБ. Первая пара этого не ловит:
// на миллионе переполнения нет, и гейт был бы зелен на арифметике, которую он якобы проверяет.
void test_a_header_that_lies_about_its_size_is_refused(const std::string& file) {
    const uint32_t lying[2][2] = {{1000000, 1000000}, {954437176, 2147483648u}};
    for (const uint32_t* head_of : lying) {
        std::vector<uint8_t> head(platformer::replay_io::HEAD);
        net::Writer w(head.data(), head.size());
        w.bytes(platformer::replay_io::MAGIC, sizeof(platformer::replay_io::MAGIC));
        w.u32(head_of[0]);
        w.u32(head_of[1]);
        check(w.ok() && rewrite(file, head), "the lying header is written");
        Stream s;
        check(!platformer::replay_io::read_file(file, s),
              "a header that lies about its size is refused");
    }
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
            test_two_rows_swapped_is_a_refusal(file, bytes);
            test_a_truncated_file_is_refused(file, bytes);
            test_a_foreign_file_is_refused(file, bytes);
            test_a_file_with_a_tail_is_refused(file, bytes);
            test_a_header_that_lies_about_its_size_is_refused(file);
        } else {
            check(false, "the written file reads as bytes");
        }
        platform::remove_file(file);
    } else {
        check(false, "the level loads for the recording");
    }
    std::printf("game-platformer-replay-io: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
