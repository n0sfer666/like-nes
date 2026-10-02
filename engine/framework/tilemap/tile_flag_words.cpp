#include "tile_flag_words.hpp"

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

bool fail(std::string& error, const std::string& message) {
    error = message;
    return false;
}

} // namespace

bool parse_flag_words(std::span<const std::string> words, TileFlags& out, std::string& error) {
    out = TILE_EMPTY;
    bool empty_word = false;
    int slope_words = 0;
    for (const std::string& word : words) {
        bool known = false;
        for (const FlagWord& w : FLAG_WORDS) {
            if (word != w.name) continue;
            if (w.bits == TILE_EMPTY) empty_word = true;
            if ((w.bits & TILE_SLOPE) != 0) ++slope_words;
            out = static_cast<TileFlags>(out | w.bits);
            known = true;
            break;
        }
        if (!known) return fail(error, "unknown tile flag '" + word + "'");
    }
    // Два склона в одной строке — не «оба сразу», а ТРЕТЬЯ ориентация: биты зеркал складываются
    // побитово, и `slope_br slope_tl` молча даёт `slope_tl`. Молчание тут хуже отказа: раскладка
    // читается глазами по легенде, а не по битам.
    if (slope_words > 1) return fail(error, "a tile has one slope orientation, not several");
    // Склон — модификатор ФОРМЫ, а не тело (`grid.hpp`): без телесного флага тайл выпадает из
    // всякого запроса, чей фильтр спрашивает `solid`, то есть выглядит как дырка в полу, а не как
    // ошибка исходника.
    if ((out & TILE_SLOPE) != 0 && (out & TILE_SOLID) == 0)
        return fail(error, "a slope needs a body flag such as 'solid'");
    // Односторонний тайл — модификатор ГРАНЕЙ по тому же основанию: сам по себе он тело не
    // объявляет, и `oneway` в одиночку дал бы тайл, невидимый всякому запросу, — то есть дырку, а
    // не платформу.
    if ((out & TILE_ONEWAY) != 0 && (out & TILE_SOLID) == 0)
        return fail(error, "a one-way tile needs a body flag such as 'solid'");
    // Склон и односторонность НЕ сочетаются, и пара отвергается здесь, а не «работает как-нибудь».
    // Держащая грань склона — гипотенуза, а правило прихода сверху мерится верхом ТАЙЛА (решение
    // владельца 2026-08-24): стоящий на нижней половине склона оказывается НИЖЕ его верха, и та же
    // грань, что держала бы его на верхней половине, перестала бы держать на нижней. Бит, который
    // в половине клетки молча не держит, хуже отказа на разборе.
    if ((out & TILE_SLOPE) != 0 && (out & TILE_ONEWAY) != 0)
        return fail(error, "a slope cannot be one-way: its holding face is the hypotenuse");
    // Лестница на склоне невыразима: лазание мерится ВЕРТИКАЛЬЮ тайла, а у гипотенузы её нет, и
    // «влез до верха» на клине пришлось бы мерить чем-то третьим.
    if ((out & TILE_LADDER) != 0 && (out & TILE_SLOPE) != 0)
        return fail(error, "a slope cannot be a ladder: climbing is measured up a tile");
    // Сплошная лестница — бит без потребителя: внутрь сплошного тайла залезть нечем. Законна она
    // ровно в паре с односторонностью — верхняя площадка, что держит сверху и пускает снизу.
    // Молча такой тайл читался бы как лестница, а работал бы как стена.
    if ((out & TILE_LADDER) != 0 && (out & TILE_SOLID) != 0 && (out & TILE_ONEWAY) == 0)
        return fail(error, "a solid ladder must be one-way: nothing climbs inside a full tile");
    // «Пусто вместе с чем-то» — противоречие, а не экзотическая запись: `empty` это отсутствие
    // флагов, и молчаливая победа второго слова означала бы, что смысл строки решает её порядок.
    if (empty_word && words.size() != 1)
        return fail(error, "'empty' means no flags and cannot be combined");
    return true;
}

} // namespace framework::tilemap
