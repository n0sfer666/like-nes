#include <cstdio>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "net_wire.hpp"
#include "platformer_replay_fixture.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"
#include "platformer_replay_io.hpp"

// Потолки ВЫДЕЛЕНИЯ у файла реплея: сколько игроков читатель согласен завести по чужому заголовку
// и сколько байт взять в память по чужому файлу. Предмет один — что читатель выпишет, ПОВЕРИВ
// присланному, — и он другой, чем у `game_platformer_replay_refusal_test`: там раскладка, то есть
// какие байты читатель отказывается понимать, здесь — какую память он отказывается занимать, ещё
// не взглянув на байты. Оба файла, которые отбиваются тут, тот гейт пропускает по построению:
// заголовок в них не лжёт ни о чём.
//
// Разрез — решение владельца по ревью аудита #21 (A·2·3): потолок размера принёс с собой границу,
// позитивный контроль на ней и пробу ёмкости, и перечисление порч вместе с ними перевалило
// жёсткий лимит длины.
//
// Цена названа, потому что она на три порядка выше соседних гейтов: контроль ровно на границе —
// это законный прогон в `MAX_TICKS` тиков, файл в 8 МБ, который пишется и читается трижды (около
// секунды на раннере). Дешевле не выходит: потолок, проверенный не на СВОЕЙ границе, закрывается
// любым более строгим числом, вплоть до единицы.
namespace {

using platformer::replay_fixture::check;
using platformer::replay_fixture::rewrite;
using platformer::replay_fixture::write_head;

using platformer::replay_io::Stream;

// Длина записи берётся у формата, а не пересчитывается: та же формула, набранная здесь второй
// раз, разошлась бы с `replay_io` молча — и гейт продолжил бы утверждать про раскладку, которой
// в дереве уже нет.
constexpr size_t ROW = platformer::input_wire::BYTES;
constexpr size_t STRIDE = static_cast<size_t>(platformer::replay_io::row_bytes(1));

// Ширина прогона против своего потолка, а не против ширины uint32. Сверка длины в читателе
// связывает `players` с размером файла только через тело, а у пустого тела этой связи нет, то есть
// гейт раскладки этот файл пропускает по построению: заголовок не ЛЖЁТ о размере, он честно
// заказывает 34 ГиБ.
//
// Граница утверждается обоими концами и обеими сторонами: ровно потолок — читается, потолок плюс
// один — отказ, и писатель такой файл не производит вовсе. Без позитивного контроля ровно на
// границе утверждение о потолке закрывалось бы любым более строгим числом.
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

// Законная строка следующего тика и заголовок, честно её объявляющий. Дописать семнадцать любых
// байт было бы другой порчей: номер тика лежит ВНУТРИ строки, и такой файл отбил бы разбор, а не
// потолок, — гейт утверждал бы про размер, а срабатывала бы раскладка.
void append_one_legal_tick(std::vector<uint8_t>& bytes, uint32_t at) {
    net::Writer head(bytes.data(), platformer::replay_io::HEAD);
    head.bytes(platformer::replay_io::MAGIC, sizeof(platformer::replay_io::MAGIC));
    head.u32(1);
    head.u32(at + 1);
    check(head.ok(), "the patched header names one tick more");

    uint8_t body[ROW] = {};
    platformer::ch::MoveInput in{};
    check(platformer::input_wire::put(body, at, in), "the extra row is encoded");
    uint8_t row[STRIDE] = {};
    net::Writer w(row, sizeof(row));
    w.bytes(body, sizeof(body));
    w.u64(0);
    check(w.ok() && w.size() == sizeof(row), "and it is a whole record");
    bytes.insert(bytes.end(), row, row + STRIDE);
}

// Файл, честный весь, но не помещающийся в память: заголовок не лжёт, все тики на месте, длина
// сходится с заявкой. Ловится здесь ровно одно — сколько байт читатель согласен взять по чужому
// файлу.
//
// Граница берётся ЗАКОННЫМИ прогонами с обеих сторон, и она ТОЧНАЯ: прогон в `MAX_TICKS` тиков
// шириной 1 занимает ровно `MAX_FILE` байт — потолок из них и посчитан. Поэтому «принять файл
// ровно в потолок» и «принять на строку больше» — разные исходы, а не одно и то же с запасом:
// строгое сравнение вместо нестрогого в читателе отбило бы первый из них, и без контроля ровно на
// границе такая правка прошла бы молча. Что граница законна, утверждается ниже размером
// записанного файла, а не ассертом о формуле: формула у неё и у потолка одна.
//
// Отказ писателя проверяется на ширине 1 и утверждается для неё же: ширину он сверяет теми же
// байтами (`bytes_for`), а прогон шириной 64 длиной в потолок тиков стоил бы гейту полгигабайта
// памяти ради второго свидетельства об одной строке кода.
//
// Файл за потолком собирается РУКАМИ, потому что писатель его больше не производит (симметрия
// потолка — решение владельца по ревью A·2·3). Это и есть тот файл, который присылают: заголовок
// его честен, а наш писатель такого не роняет.
//
// Проба ёмкости называет, что потолок спрашивается НА КАЖДОМ куске: у отказавшего чтения вектор не
// вырос вовсе. Перенеси проверку за цикл — файл так же отобьётся, а вектор подержит в себе все
// восемь мегабайт, то есть ровно то выделение, ради запрета которого потолок и стоит.
//
// Последняя строка называет, ГДЕ потолок стоит: те же байты, взятые в память мимо `read_file`,
// разбираются без единой жалобы. Перенеси проверку размера в `parse` — гейт на одном лишь
// `read_file` остался бы зелёным, а чужие мегабайты как приезжали бы в память до отказа, так и
// приезжали.
void test_the_file_has_a_size_ceiling(const std::string& file) {
    constexpr uint32_t FITS = platformer::replay_io::MAX_TICKS;
    Stream fits;
    fits.reset(1);
    platformer::ch::MoveInput in{};
    bool recorded = true;
    for (uint32_t t = 0; t < FITS && recorded; ++t) recorded = fits.record(&in, t);
    check(recorded, "the longest run that still fits under the ceiling is recorded");
    check(platformer::replay_io::write_file(file, fits), "and the writer accepts it");
    Stream under;
    check(platformer::replay_io::read_file(file, under),
          "control: a file exactly as big as the ceiling allows is read");
    check(under.ticks() == FITS, "and it is the run its header named");

    check(fits.record(&in, FITS), "one tick more is recorded");
    check(!platformer::replay_io::write_file(file, fits),
          "but the writer refuses to produce a file it would refuse to read back");

    std::vector<uint8_t> raw;
    check(platform::read_bytes(file, raw), "the file at the ceiling still reads as plain bytes");
    check(raw.size() == platformer::replay_io::MAX_FILE, "and it is exactly the ceiling long");
    // Дальше файл ПРАВИТСЯ по месту, и предпосылка о его длине обязана держать: `check` только
    // считает отказы, поэтому без выхода патч заголовка ушёл бы в чужой буфер, а на диск легли бы
    // семнадцать байт — гейт покраснел бы каскадом про раскладку, ни словом не назвав причину.
    if (raw.size() != platformer::replay_io::MAX_FILE) return;
    append_one_legal_tick(raw, FITS);
    check(rewrite(file, raw), "an honest file one tick past the ceiling is written back");
    Stream over;
    check(!platformer::replay_io::read_file(file, over),
          "a file one tick past the ceiling is refused");

    std::vector<uint8_t> probe;
    check(!platform::read_bytes_capped(file, probe, STRIDE),
          "the same file under a ceiling of one record is refused too");
    check(probe.capacity() < platformer::replay_io::MAX_FILE / 64,
          "and the refusal happened per chunk: the vector never grew to hold the file");

    Stream by_bytes;
    check(platformer::replay_io::parse(raw, by_bytes),
          "and those same bytes parse: the ceiling lives at the READ, not in the parser");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    (void)argc;

    std::printf("platformer sample: what the replay reader refuses to allocate\n");
    const std::string file =
        platform::exe_path() + ".replay" + std::to_string(platform::process_id());
    test_the_width_of_a_run_has_a_ceiling(file);
    test_the_file_has_a_size_ceiling(file);
    platform::remove_file(file);
    std::printf("game-platformer-replay-ceiling: %s\n",
                platformer::replay_fixture::fails == 0 ? "PASS" : "FAIL");
    return platformer::replay_fixture::fails == 0 ? 0 : 1;
}
