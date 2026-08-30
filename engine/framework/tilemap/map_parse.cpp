#include <cstring>

#include "map_bake.hpp"
#include "map_text.hpp"

// Грамматика исходника: сколько полей в строке, что в них лежит и что нельзя решить по одной
// строке. Отделено от сборки байтов (`map_bake.cpp`) по той же границе, что `profile_parse.cpp` от
// `profile_bake.cpp`: раскладка таблицы пиннута static_assert'ами и едет вместе с версией формата,
// а грамматика текста живёт своей жизнью.
namespace framework::tilemap {
namespace {

// Словарь флагов — ТАБЛИЦА, а не цепочка сравнений: бит формата и слово исходника обязаны быть
// одним местом, иначе бит, заведённый в `grid.hpp`, читается из карты, но не пишется в неё.
struct FlagWord {
    const char* name;
    TileFlags bits;
};

const FlagWord FLAG_WORDS[] = {
    {"empty", TILE_EMPTY},
    {"solid", TILE_SOLID},
    // Ориентация склона названа СПЛОШНЫМ УГЛОМ (`grid.hpp`): `br` — прямой угол внизу справа.
    // Зеркальные биты по отдельности словом не пишутся вовсе, поэтому «отражение без склона»
    // невыразимо, и отвергать его нечем и незачем.
    {"slope_br", TILE_SLOPE},
    {"slope_bl", TILE_SLOPE | TILE_SLOPE_FLIP_X},
    {"slope_tr", TILE_SLOPE | TILE_SLOPE_FLIP_Y},
    {"slope_tl", TILE_SLOPE | TILE_SLOPE_FLIP_X | TILE_SLOPE_FLIP_Y},
    // Односторонний тайл — модификатор того, какие грани держат, и пишется он ВМЕСТЕ с телом
    // (`solid oneway`): бит живёт рядом с `TILE_SOLID`, а не вместо него, чтобы запрос, спросивший
    // `solid`, платформу видел, а держать её решал по грани хита.
    {"oneway", TILE_ONEWAY},
    // Лестница — метка РЕЖИМА движения, а не тела (`grid.hpp`): пишется одна (`ladder`) там, где
    // сквозь неё ходят, и с односторонней площадкой (`solid oneway ladder`) там, где на неё встают.
    {"ladder", TILE_LADDER},
};

constexpr uint32_t SEEN_TILE_SIZE = 1u << 0;
constexpr uint32_t SEEN_ORIGIN = 1u << 1;
// Потолок карты: смещения формата 32-битные, и карта, чья таблица не влезает в uint32, испеклась бы
// с обёрнутым смещением, то есть прочиталась бы как другая карта. Четыре миллиона тайлов — это
// 8 МБ флагов и экран платформера в две тысячи ширин.
constexpr uint64_t MAX_MAP_TILES = 1ull << 22;

struct Legend {
    char glyph;
    TileFlags flags;
};

bool fail(MapBakeError& err, int line, const std::string& message) {
    err.line = line;
    err.message = message;
    return false;
}

bool parse_flag_words(const std::vector<std::string>& f, TileFlags& out, int line,
                      MapBakeError& err) {
    out = TILE_EMPTY;
    bool empty_word = false;
    int slope_words = 0;
    for (std::size_t i = 2; i < f.size(); ++i) {
        bool known = false;
        for (const FlagWord& w : FLAG_WORDS) {
            if (f[i] != w.name) continue;
            if (w.bits == TILE_EMPTY) empty_word = true;
            if ((w.bits & TILE_SLOPE) != 0) ++slope_words;
            out = static_cast<TileFlags>(out | w.bits);
            known = true;
            break;
        }
        if (!known) return fail(err, line, "unknown tile flag '" + f[i] + "'");
    }
    // Два склона в одной строке — не «оба сразу», а ТРЕТЬЯ ориентация: биты зеркал складываются
    // побитово, и `slope_br slope_tl` молча даёт `slope_tl`. Молчание тут хуже отказа: раскладка
    // читается глазами по легенде, а не по битам.
    if (slope_words > 1) return fail(err, line, "a tile has one slope orientation, not several");
    // Склон — модификатор ФОРМЫ, а не тело (`grid.hpp`): без телесного флага тайл выпадает из
    // всякого запроса, чей фильтр спрашивает `solid`, то есть выглядит как дырка в полу, а не как
    // ошибка исходника.
    if ((out & TILE_SLOPE) != 0 && (out & TILE_SOLID) == 0)
        return fail(err, line, "a slope needs a body flag such as 'solid'");
    // Односторонний тайл — модификатор ГРАНЕЙ по тому же основанию: сам по себе он тело не
    // объявляет, и `oneway` в одиночку дал бы тайл, невидимый всякому запросу, — то есть дырку, а
    // не платформу.
    if ((out & TILE_ONEWAY) != 0 && (out & TILE_SOLID) == 0)
        return fail(err, line, "a one-way tile needs a body flag such as 'solid'");
    // Склон и односторонность НЕ сочетаются, и пара отвергается здесь, а не «работает как-нибудь».
    // Держащая грань склона — гипотенуза, а правило прихода сверху мерится верхом ТАЙЛА (решение
    // владельца 2026-08-24): стоящий на нижней половине склона оказывается НИЖЕ его верха, и та же
    // грань, что держала бы его на верхней половине, перестала бы держать на нижней. Бит, который
    // в половине клетки молча не держит, хуже отказа на разборе.
    if ((out & TILE_SLOPE) != 0 && (out & TILE_ONEWAY) != 0)
        return fail(err, line, "a slope cannot be one-way: its holding face is the hypotenuse");
    // Лестница на склоне невыразима: лазание мерится ВЕРТИКАЛЬЮ тайла, а у гипотенузы её нет, и
    // «влез до верха» на клине пришлось бы мерить чем-то третьим.
    if ((out & TILE_LADDER) != 0 && (out & TILE_SLOPE) != 0)
        return fail(err, line, "a slope cannot be a ladder: climbing is measured up a tile");
    // Сплошная лестница — бит без потребителя: внутрь сплошного тайла залезть нечем. Законна она
    // ровно в паре с односторонностью — верхняя площадка, что держит сверху и пускает снизу.
    // Молча такой тайл читался бы как лестница, а работал бы как стена.
    if ((out & TILE_LADDER) != 0 && (out & TILE_SOLID) != 0 && (out & TILE_ONEWAY) == 0)
        return fail(err, line, "a solid ladder must be one-way: nothing climbs inside a full tile");
    // «Пусто вместе с чем-то» — противоречие, а не экзотическая запись: `empty` это отсутствие
    // флагов, и молчаливая победа второго слова означала бы, что смысл строки решает её порядок.
    if (empty_word && f.size() != 3)
        return fail(err, line, "'empty' means no flags and cannot be combined");
    return true;
}

// Закрытие карты: то, о чём нельзя судить по одной строке. Недостающее называется ПОИМЁННО и
// строкой, на которой карта кончилась, — «map 'field' is missing tile_size» ведёт к правке, а
// «bad map» ведёт к чтению исходников пекаря.
bool close_map(std::vector<ParsedMap>& out, uint32_t seen, int line, MapBakeError& err) {
    if (out.empty()) return true;
    const ParsedMap& m = out.back();
    if ((seen & SEEN_TILE_SIZE) == 0)
        return fail(err, line, "map '" + m.name + "' is missing tile_size");
    if ((seen & SEEN_ORIGIN) == 0) return fail(err, line, "map '" + m.name + "' is missing origin");
    if (m.height == 0) return fail(err, line, "map '" + m.name + "' has no row");
    if (static_cast<uint64_t>(m.width) * m.height > MAX_MAP_TILES)
        return fail(err, line, "map '" + m.name + "' is larger than the format allows");
    return true;
}

bool assign_row(ParsedMap& m, const std::vector<Legend>& legend, const std::string& body, int line,
                MapBakeError& err) {
    if (body.empty()) return fail(err, line, "a map row cannot be empty");
    if (m.height == 0)
        m.width = static_cast<uint32_t>(body.size());
    else if (body.size() != m.width)
        return fail(err, line, "this row is not as wide as the first one");
    for (char c : body) {
        const Legend* found = nullptr;
        for (const Legend& l : legend)
            if (l.glyph == c) found = &l;
        if (found == nullptr)
            return fail(err, line, std::string("glyph '") + c + "' is not in the legend");
        m.flags.push_back(found->flags);
    }
    ++m.height;
    return true;
}

bool assign(ParsedMap& m, std::vector<Legend>& legend, uint32_t& seen,
            const std::vector<std::string>& f, int line, MapBakeError& err) {
    if (f[0] == "tile_size") {
        if (f.size() != 2) return fail(err, line, "expected 'tile_size | <number>'");
        if ((seen & SEEN_TILE_SIZE) != 0) return fail(err, line, "tile_size is set twice");
        if (!map_parse_fix(f[1], m.tile_size))
            return fail(err, line, "tile_size must be a decimal number");
        // Сетка приводит размер сама (`grid.cpp`): нечётный raw оставляет щель между тайлами,
        // нулевой схлопывает карту в ячейку. Приведённое молча значение сделало бы исходник
        // враньём, поэтому оно отвергается здесь.
        if (m.tile_size.raw < 2 || (m.tile_size.raw & 1) != 0)
            return fail(err, line, "tile_size must be at least 2 raw units and an even number");
        seen |= SEEN_TILE_SIZE;
        return true;
    }
    if (f[0] == "origin") {
        if (f.size() != 3) return fail(err, line, "expected 'origin | <x> | <y>'");
        if ((seen & SEEN_ORIGIN) != 0) return fail(err, line, "origin is set twice");
        if (!map_parse_fix(f[1], m.origin.x) || !map_parse_fix(f[2], m.origin.y))
            return fail(err, line, "origin takes two decimal numbers");
        seen |= SEEN_ORIGIN;
        return true;
    }
    if (f[0] == "legend") {
        if (f.size() < 3 || f[1].size() != 1)
            return fail(err, line, "expected 'legend | <glyph> | <flag>...' with one visible glyph");
        // Проверки на `|` и `#` тут нет и быть не может: разделитель до поля не доезжает, а хвост
        // после `#` срезан комментарием — обе записи разбираются как «глиф не одна видимая
        // буква» строкой выше. Ветка, которую не выполнить, в пути голдена не нужна.
        const char g = f[1][0];
        for (const Legend& l : legend)
            if (l.glyph == g)
                return fail(err, line, std::string("glyph '") + g + "' is declared twice");
        TileFlags bits = TILE_EMPTY;
        if (!parse_flag_words(f, bits, line, err)) return false;
        legend.push_back(Legend{g, bits});
        return true;
    }
    if (f[0] == "row") {
        if (f.size() != 2) return fail(err, line, "expected 'row | <glyphs>'");
        return assign_row(m, legend, f[1], line, err);
    }
    return fail(err, line, "unknown key '" + f[0] + "'");
}

} // namespace

bool parse_maps(const std::string& text, std::vector<ParsedMap>& out, MapBakeError& err) {
    out.clear();
    std::vector<Legend> legend;
    uint32_t seen = 0;
    int line = 0;
    // Номер строки для отказа «на закрытии карты» берётся у последней СОДЕРЖАТЕЛЬНОЙ строки, а не у
    // счётчика: исходник обычно кончается переводом строки, и счётчик показывал бы на строку ЗА
    // концом файла — то есть отказ вёл бы ровно туда, куда номер строки существует, чтобы не
    // пускать.
    int content_line = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        std::string raw = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
        ++line;
        const std::size_t hash = raw.find('#');
        if (hash != std::string::npos) raw.erase(hash);
        const std::string body = map_trim(raw);
        if (body.empty()) continue;
        const int prev_content = content_line;
        content_line = line;

        const std::vector<std::string> f = map_split(body);
        if (f[0].empty()) return fail(err, line, "expected '<key> | <value>'");
        if (f[0] == "map") {
            if (f.size() != 2 || f[1].empty()) return fail(err, line, "expected 'map | <name>'");
            if (!close_map(out, seen, prev_content > 0 ? prev_content : line, err)) return false;
            for (const ParsedMap& m : out)
                if (m.name == f[1]) return fail(err, line, "map '" + f[1] + "' is declared twice");
            out.push_back(ParsedMap{});
            out.back().name = f[1];
            // Легенда ПРИНАДЛЕЖИТ карте, а не файлу: общая на две карты, она позволила бы второй
            // молча унаследовать глифы первой, и правка легенды наверху меняла бы уровень внизу.
            legend.clear();
            seen = 0;
            continue;
        }
        if (out.empty()) return fail(err, line, "a line before the first 'map' line");
        if (!assign(out.back(), legend, seen, f, line, err)) return false;
    }
    const int end_line = content_line > 0 ? content_line : line;
    if (!close_map(out, seen, end_line, err)) return false;
    // Пустой исходник — находка, а не законный «ноль карт»: таблица без карт читается без ошибки и
    // означает «уровня нет», то есть отдаёт отладку в рантайм.
    if (out.empty()) return fail(err, end_line, "the source declares no map");
    return true;
}

} // namespace framework::tilemap
