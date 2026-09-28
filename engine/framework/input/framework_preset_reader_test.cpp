#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform_args.hpp"
#include "preset_bake.hpp"
#include "preset_format.hpp"
#include "presets.hpp"

// Что ЧИТАТЕЛЬ секции обязан отбить. Отдельная цель от отказов бейка (`framework_preset_refusal_test`)
// по тому же основанию, по которому та отделена от round-trip: предмет здесь — байты секции, а не
// текст манифеста, и сломанная проверка даёт не отказ с номером строки, а порченый бандл, читаемый
// как чужая память. Каждая фикстура — испечённая таблица с ОДНОЙ правкой: секция, собранная руками
// целиком, сверялась бы сама с собой.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

// Неиспёкшаяся фикстура останавливает прогон: правки ниже пишут по смещениям из её заголовка, и в
// пустом буфере набор ронял бы сам себя порчей памяти, а не ловил чужую.
bool baked(const std::string& text, std::vector<uint8_t>& out) {
    framework::input::PresetBakeError err;
    if (framework::input::bake_presets(text, out, err)) return true;
    std::printf("  FAIL: a fixture did not bake: line %d: %s\n", err.line, err.message.c_str());
    return false;
}

// Пресеты по счёту: потолок читателя проверяется таблицей, испечённой ровно в него.
std::string many_presets(uint32_t n) {
    std::string m;
    for (uint32_t i = 0; i < n; ++i)
        m += "preset | p" + std::to_string(i) + "\naxis | x | key:d | key:a\n";
    return m;
}

void put32(std::vector<uint8_t>& b, std::size_t at, uint32_t v) {
    std::memcpy(b.data() + at, &v, sizeof(v));
}

// Адрес поля строки `row` таблицы, чьё смещение заголовок держит по адресу `table`.
template <typename Row>
std::size_t field_at(const std::vector<uint8_t>& b, std::size_t table, uint32_t row,
                     std::size_t field) {
    uint32_t base = 0;
    std::memcpy(&base, b.data() + table, sizeof(base));
    return base + row * sizeof(Row) + field;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    using namespace framework::input;

    // Одна ось строками-дубликатами ровно до потолка: упирается в потолок строк, не задевая предел
    // логических осей. Второй пресет даёт строку, на которую первый потом растянут.
    std::string cap_text = "preset | p\n";
    for (uint32_t i = 0; i < MAX_AXIS_ROWS; ++i) cap_text += "axis | x | key:d | key:a\n";
    // Имя ровно в потолок, и в блобе за ним лежит ЕЩЁ одна строка: без второй порча имени ловилась
    // бы хвостовым нулём блоба, то есть не потолком.
    const std::string long_name = "preset | " + std::string(MAX_NAME, 'p') + "\n";
    // Три строки на две логические оси плюс второй пресет: и число строк пресета, и число строк
    // таблицы больше числа логических осей, так что каждая из этих границ пропустила бы номер пары,
    // которого у пресета нет. Пара приезжает формой: `pair` пекарь пишет ЛОГИЧЕСКИМ номером.
    const std::string pair_text =
        "preset | p\naxis | move_x | key:d | key:a\naxis | move_x | key:l | key:j\n"
        "axis | move_y | key:w | key:s\nshape | move_x | 0.0 | 1.0 | 1 | move_y\n"
        "preset | q\naxis | move_z | key:e | key:q\n";
    // Пад с сопоставлением по имени: у его строки ДВА имени в блобе, и оба читатель обязан мерить.
    const std::string pad_text =
        "preset | p\naxis | move_x | key:d | key:a\npad | Some Pad | - | - | Some | xbox | 0.18 | 0.12\n";
    std::vector<uint8_t> blob, cap, cur, name, pr, pad, many;
    if (!baked("preset | p\naxis | move_x | key:d | key:a\n", blob) ||
        !baked(cap_text + "preset | q\naxis | y | key:w | key:s\n", cap) ||
        !baked("preset | p\naction | jump | key:space\n"
               "axis | move_x | key:d | key:a\naxis | move_y | key:w | key:s\n", cur) ||
        !baked(long_name + "axis | move_x | key:d | key:a\n", name) || !baked(pair_text, pr) ||
        !baked(pad_text, pad) || !baked(many_presets(MAX_PRESETS), many)) {
        std::printf("framework-preset-reader: FAIL\n");
        return 1;
    }

    PresetTable good;
    check(good.open(blob.data(), blob.size()), "the untouched table is the positive control");
    PresetTable bad;
    std::vector<uint8_t> broken;
    const auto opens_with = [&](const std::vector<uint8_t>& src, std::size_t at, uint32_t v) {
        broken = src;
        put32(broken, at, v);
        return bad.open(broken.data(), broken.size());
    };
    // Смещение таблицы проверяется снизу и на выравнивание, а не только на «влезает». Нулевое
    // кладёт строки поверх заголовка; нечётное даёт `reinterpret_cast` мимо границы. Фикстура
    // невыровненности собрана сдвигом СОДЕРЖИМОГО вместе со всеми смещениями заголовка — правка
    // одного поля отбивается проверкой размера, и утверждение вышло бы вакуумным.
    broken = blob;
    put32(broken, offsetof(PresetHeader, presets_offset), 0);
    check(!bad.open(broken.data(), broken.size()), "a table overlapping the header is rejected");
    broken = blob;
    broken.insert(broken.begin() + sizeof(PresetHeader), 2, 0);
    for (std::size_t at : {offsetof(PresetHeader, presets_offset),
                           offsetof(PresetHeader, actions_offset),
                           offsetof(PresetHeader, axes_offset),
                           offsetof(PresetHeader, bindings_offset),
                           offsetof(PresetHeader, pads_offset),
                           offsetof(PresetHeader, strings_offset),
                           offsetof(PresetHeader, total_size)}) {
        uint32_t v = 0;
        std::memcpy(&v, broken.data() + at, sizeof(v));
        put32(broken, at, v + 2);
    }
    check(!bad.open(broken.data(), broken.size()), "a misaligned table offset is rejected");
    // Блоб имён снизу: смещение 0 кладёт имена на магию и версию. Ноль в заголовке находится
    // (счётчики короткие), поэтому потолок имён такую таблицу пропускает — отбить её может только
    // нижняя граница блоба (аудит #21, ревью A·2).
    check(!opens_with(blob, offsetof(PresetHeader, strings_offset), 0),
          "a string blob starting on the header is rejected");
    // Таблица ЗА итогом заголовка, но внутри буфера вызывающего: байты секции кончились, а буфер
    // длиннее — так его и передаёт бандл. Строка осей скопирована байт в байт, то есть законна во
    // всём, кроме места: отбить её может только потолок `total_size`.
    broken = blob;
    while (broken.size() % alignof(AxisRow) != 0) broken.push_back(0);
    const std::size_t past = broken.size();
    uint32_t axes_off = 0;
    std::memcpy(&axes_off, blob.data() + offsetof(PresetHeader, axes_offset), sizeof(axes_off));
    broken.insert(broken.end(), blob.begin() + axes_off,
                  blob.begin() + axes_off + sizeof(AxisRow));
    put32(broken, offsetof(PresetHeader, axes_offset), static_cast<uint32_t>(past));
    check(!bad.open(broken.data(), broken.size()),
          "a table past the header's total size is rejected even inside the caller's buffer");

    // Потолок строк осей у ЧИТАТЕЛЯ: пресет растянут на строку соседа, так что срез честно лежит в
    // таблице и отбить его может только потолок, а не граница массива.
    check(good.open(cap.data(), cap.size()), "a preset holding exactly the row cap opens");
    const std::size_t presets = offsetof(PresetHeader, presets_offset);
    put32(cap, field_at<PresetRow>(cap, presets, 0, offsetof(PresetRow, axis_count)),
          MAX_AXIS_ROWS + 1);
    check(!bad.open(cap.data(), cap.size()), "a preset one axis row over the cap is rejected");

    // Курсоры-срезы (A·2·1): заголовок честен и массивы влезают, врёт одна строка — ровно на единицу
    // за границей. `begin = 0xFFFFFFFF` при `count = 1` в uint32 заворачивается в ноль и проходит
    // проверку, которая считает не в uint64.
    check(good.open(cur.data(), cur.size()), "the cursor fixture opens untouched");
    check(!opens_with(cur, field_at<PresetRow>(cur, presets, 0, offsetof(PresetRow, action_begin)),
                      0xFFFFFFFFu),
          "an action slice wrapping around uint32 is rejected");
    check(!opens_with(cur, field_at<PresetRow>(cur, presets, 0, offsetof(PresetRow, axis_begin)), 1),
          "an axis slice one row past the table is rejected");
    check(!opens_with(cur, field_at<ActionRow>(cur, offsetof(PresetHeader, actions_offset), 0,
                                               offsetof(ActionRow, binding_begin)), 1),
          "a binding slice one row past the table is rejected");

    // Номер пары (A·2·1) считается по ЛОГИЧЕСКИМ осям пресета, а не по его строкам: у первого
    // пресета фикстуры три строки, две оси, и номер 2 указывает на ось, которой нет, — по нему
    // радиальная зона читала бы соседнюю строку как вторую половину стика.
    check(good.open(pr.data(), pr.size()), "the pair fixture opens untouched");
    const std::size_t pair0 = field_at<AxisRow>(pr, offsetof(PresetHeader, axes_offset), 0,
                                                offsetof(AxisRow, pair_axis));
    check(!opens_with(pr, pair0, 2), "a pair axis past the preset's logical axes is rejected");
    check(opens_with(pr, pair0, 1), "a pair axis naming the preset's last logical axis still opens");
    // Ось, спаренная САМА С СОБОЙ, проходит границу `pair < число логических осей` насквозь: номер
    // законный, смысла нет — радиальная зона считалась бы по одной оси дважды (аудит #21, ревью A·2).
    check(!opens_with(pr, pair0, 0), "a pair axis naming its own axis is rejected");

    // Потолок имени (A·2·1b): у имени ровно в потолок затирается его ноль, и первый ноль уезжает за
    // потолок, потому что следом в блобе лежит имя оси. Читатель обязан отбить такую таблицу: имена
    // он сравнивает ДРУГ С ДРУГОМ, и имя без конца — это цена старта, линейная по размеру секции.
    check(good.open(name.data(), name.size()), "a name exactly at the cap opens");
    uint32_t name_at = 0, strings_at = 0;
    std::memcpy(&name_at,
                name.data() + field_at<PresetRow>(name, presets, 0, offsetof(PresetRow, name_offset)),
                sizeof(name_at));
    std::memcpy(&strings_at, name.data() + offsetof(PresetHeader, strings_offset),
                sizeof(strings_at));
    name.at(strings_at + name_at + MAX_NAME) = 'p';
    check(!bad.open(name.data(), name.size()), "a name without a terminator inside the cap is rejected");

    // Смещение имени ЗА блобом — вторая половина той же проверки: `strcmp` по нему ушёл бы в чужую
    // память. Оба конца — за концом блоба и 0xFFFFFFFF — отбивает ОДИН затвор, `offset >= size` в
    // `name_fits`: до арифметики `size - offset`, где второе заворачивалось бы, дело не доходит.
    const std::size_t name0 = field_at<PresetRow>(blob, presets, 0, offsetof(PresetRow, name_offset));
    uint32_t blob_strings = 0, blob_total = 0;
    std::memcpy(&blob_strings, blob.data() + offsetof(PresetHeader, strings_offset),
                sizeof(blob_strings));
    std::memcpy(&blob_total, blob.data() + offsetof(PresetHeader, total_size), sizeof(blob_total));
    const uint32_t strings_size = blob_total - blob_strings;
    check(!opens_with(blob, name0, strings_size), "a name offset at the end of the blob is rejected");
    check(!opens_with(blob, name0, 0xFFFFFFFFu), "a name offset wrapping around uint32 is rejected");
    // Позитивный контроль той же границы: последний байт блоба — его завершающий ноль, то есть
    // законное пустое имя. Отбей читатель и его — проверка стояла бы на единицу не там.
    check(opens_with(blob, name0, strings_size - 1),
          "a name offset at the blob's last byte still opens: it is the empty name");

    // Имена пада — те же имена блоба: до ревью A·2 их не мерил никто, и `profile_for` сравнивал с
    // именем устройства строку, начинающуюся за концом секции.
    check(good.open(pad.data(), pad.size()), "the pad fixture opens untouched");
    const std::size_t pads = offsetof(PresetHeader, pads_offset);
    check(!opens_with(pad, field_at<PadRow>(pad, pads, 0, offsetof(PadRow, name_offset)),
                      0xFFFFFFFFu),
          "a pad name offset outside the blob is rejected");
    check(!opens_with(pad, field_at<PadRow>(pad, pads, 0, offsetof(PadRow, match_offset)),
                      0xFFFFFFFFu),
          "a pad name match offset outside the blob is rejected");

    // Потолок пресетов (ревью A·2). Лишний пресет дописывается НУЛЕВОЙ строкой, а все смещения за
    // таблицей сдвигаются на её размер: строка с пустым именем и пустыми срезами законна во всём
    // остальном, поэтому отбить такую таблицу может только сам потолок. Правка одного счётчика в
    // заголовке ловилась бы мусором на месте лишней строки, то есть утверждала бы не то.
    check(good.open(many.data(), many.size()), "a table holding exactly the preset cap opens");
    uint32_t presets_off = 0;
    std::memcpy(&presets_off, many.data() + presets, sizeof(presets_off));
    broken = many;
    broken.insert(broken.begin() + presets_off + MAX_PRESETS * sizeof(PresetRow), sizeof(PresetRow),
                  0);
    for (std::size_t at : {offsetof(PresetHeader, actions_offset),
                           offsetof(PresetHeader, axes_offset),
                           offsetof(PresetHeader, bindings_offset),
                           offsetof(PresetHeader, pads_offset),
                           offsetof(PresetHeader, strings_offset),
                           offsetof(PresetHeader, total_size)}) {
        uint32_t v = 0;
        std::memcpy(&v, broken.data() + at, sizeof(v));
        put32(broken, at, v + static_cast<uint32_t>(sizeof(PresetRow)));
    }
    put32(broken, offsetof(PresetHeader, preset_count), MAX_PRESETS + 1);
    check(!bad.open(broken.data(), broken.size()),
          "a table one preset over the cap is rejected, honest as its extra row is");

    const bool pass = (fails == 0);
    std::printf("framework-preset-reader: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
