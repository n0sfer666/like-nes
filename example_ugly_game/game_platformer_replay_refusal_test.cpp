#include <cstdio>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "platformer_replay_fixture.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "platformer_replay_io.hpp"

// Тотальность ЧИТАТЕЛЯ формата реплея: чужой файл, порченный шестью разными способами, обязан быть
// отбит, а не разобран во что-нибудь правдоподобное.
//
// Отдельной целью от `game_platformer_replay_io_test` по предмету: там спрашивают, доносит ли файл
// ТОТ ЖЕ прогон до верификатора, и потому нужны бандл, сцена и переигровка; здесь не нужно ничего,
// кроме байт. Разделение — решение владельца по ревью аудита #21 (A·2·3), когда перечисление порч
// перевалило мягкий лимит длины.
//
// Эталон здесь СОБИРАЕТСЯ, а не записывается прогоном образца: второй вызов `replay_run::record`
// был бы второй правдой о том, что такое «тот же прогон», и тянул бы сюда бандл ради фикстуры,
// у которой предмет — раскладка, а не маршрут. Порчи ниже все про раскладку.
namespace {

using platformer::replay_fixture::check;
using platformer::replay_fixture::rewrite;
using platformer::replay_fixture::write_head;

using platformer::replay_io::Stream;

constexpr size_t ROW = platformer::input_wire::BYTES;
constexpr size_t STRIDE = ROW + platformer::replay_io::CLAIM;
constexpr uint32_t TICKS = 8;

// Строки РАЗНЫЕ между собой: гейт перестановки утверждает, что две соседние строки нельзя поменять
// местами незаметно, и на одинаковых строках он был бы зелен независимо от читателя.
bool build_honest(Stream& s) {
    s.reset(1);
    for (uint32_t t = 0; t < TICKS; ++t) {
        platformer::ch::MoveInput in{};
        in.move_x = fix32::from_int(static_cast<int>(t % 3) - 1);
        in.jump_held = (t % 2) == 0;
        in.down_held = (t % 3) == 0;
        if (!s.record(&in, 0x1000u + t)) return false;
    }
    return true;
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
        check(write_head(file, head_of[0], head_of[1]), "the lying header is written");
        Stream s;
        check(!platformer::replay_io::read_file(file, s),
              "a header that lies about its size is refused");
    }
}

// Ширина прогона против своего потолка, а не против ширины uint32. Сверка длины выше связывает
// `players` с размером файла только через тело, а у пустого тела этой связи нет, то есть предыдущий
// гейт этот файл пропускает по построению: заголовок не ЛЖЁТ о размере, он честно заказывает 34 ГиБ.
//
// Граница утверждается обоими концами и обеими сторонами: ровно потолок — читается, потолок плюс
// один — отказ, и писатель такой файл не производит вовсе. Без позитивного контроля ровно на границе
// утверждение о потолке закрывалось бы любым более строгим числом, вплоть до единицы.
void test_the_width_of_a_run_has_a_ceiling(const std::string& file) {
    constexpr uint32_t MAX = platformer::replay_io::MAX_PLAYERS;
    check(write_head(file, MAX + 1, 0), "the header one player over the ceiling is written");
    Stream over;
    check(!platformer::replay_io::read_file(file, over),
          "an empty run one player wider than the ceiling is refused");

    check(write_head(file, MAX, 0), "the header exactly at the ceiling is written");
    Stream edge;
    check(platformer::replay_io::read_file(file, edge),
          "control: an empty run exactly at the ceiling is read");
    check(edge.players() == MAX && edge.ticks() == 0, "and it is the width its header named");

    // Писатель утверждается ОБОИМИ концами так же, как читатель: без контроля ровно на потолке
    // проверку в писателе можно было бы ужесточить до «шире одного игрока не пишем», и гейт остался
    // бы зелёным — ровно та вакуозность, от которой контроль читателя уже защищён. Контроль берётся
    // с записанным тиком, а не на пустом прогоне: иначе он держался бы ещё и на том, что писать
    // пустой прогон вообще позволено, и покраснел бы от запрета, к потолку отношения не имеющего.
    Stream at;
    at.reset(MAX);
    std::vector<platformer::ch::MoveInput> row(MAX);
    check(at.record(row.data(), 0), "a run exactly at the ceiling records a tick");
    check(at.players() == MAX, "and it is that wide");
    check(platformer::replay_io::write_file(file, at),
          "control: the writer accepts a run exactly at the ceiling");

    Stream wide;
    wide.reset(MAX + 1);
    check(wide.players() == MAX + 1, "a run one player over the ceiling is that wide");
    check(!platformer::replay_io::write_file(file, wide),
          "and the writer refuses that width, just as it refuses a run with no players");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    (void)argc;

    std::printf("platformer sample: what the replay reader refuses\n");
    const std::string file =
        platform::exe_path() + ".replay" + std::to_string(platform::process_id());
    Stream honest;
    if (build_honest(honest) && platformer::replay_io::write_file(file, honest)) {
        std::vector<uint8_t> bytes;
        if (platform::read_bytes(file, bytes)) {
            test_two_rows_swapped_is_a_refusal(file, bytes);
            test_a_truncated_file_is_refused(file, bytes);
            test_a_foreign_file_is_refused(file, bytes);
            test_a_file_with_a_tail_is_refused(file, bytes);
            test_a_header_that_lies_about_its_size_is_refused(file);
            test_the_width_of_a_run_has_a_ceiling(file);
        } else {
            check(false, "the written file reads as bytes");
        }
        platform::remove_file(file);
    } else {
        check(false, "the fixture run is built and written");
    }
    std::printf("game-platformer-replay-refusal: %s\n", platformer::replay_fixture::fails == 0 ? "PASS" : "FAIL");
    return platformer::replay_fixture::fails == 0 ? 0 : 1;
}
