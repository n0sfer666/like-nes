#include "atlas_bake.hpp"
#include "text_fields.hpp"
#include "clip.hpp"

// Грамматика исходника: сколько полей в строке, что в них лежит и что нельзя решить по одной
// строке. Отделено от сборки байтов (`atlas_bake.cpp`) по той же границе, что `map_parse.cpp` от
// `map_bake.cpp`: раскладка таблицы пиннута static_assert'ами и едет вместе с версией формата, а
// грамматика текста живёт своей жизнью.
namespace framework::graphics {
namespace {

// Потолок регионов — это ТИП `RegionId`, а не запас на вырост: кадр клипа ссылается на регион
// шестнадцатибитным номером, и 65536-й регион получил бы номер нулевого. Число берётся У ТИПА, а не
// пишется литералом: расширь `RegionId` — и литерал остался бы прежним, отбивая законные регионы.
constexpr std::size_t MAX_REGIONS = std::size_t{1} << (8 * sizeof(RegionId));

bool fail(AtlasBakeError& err, int line, const std::string& message) {
    err.line = line;
    err.message = message;
    return false;
}

bool parse_page(ParsedAtlas& out, const std::vector<std::string>& f, int line,
                AtlasBakeError& err) {
    if (f.size() != 3) return fail(err, line, "expected 'atlas | <width> | <height>'");
    if (out.page_width != 0) return fail(err, line, "the page size is set twice");
    if (!core::parse_u16(f[1], out.page_width) || !core::parse_u16(f[2], out.page_height))
        return fail(err, line, "the page size takes two whole numbers of pixels");
    if (out.page_width == 0 || out.page_height == 0) {
        out.page_width = 0;
        return fail(err, line, "a page with a zero side holds no region");
    }
    return true;
}

bool parse_region(ParsedAtlas& out, const std::vector<std::string>& f, int line,
                  AtlasBakeError& err) {
    if (f.size() != 8)
        return fail(err, line, "expected 'region | <name> | <x> | <y> | <w> | <h> | <px> | <py>'");
    if (f[1].empty()) return fail(err, line, "a region needs a name: it is the stable reference");
    for (const ParsedRegion& r : out.regions)
        if (r.name == f[1]) return fail(err, line, "region '" + f[1] + "' is declared twice");
    if (out.regions.size() == MAX_REGIONS)
        return fail(err, line, "more regions than a 16-bit RegionId can name");

    ParsedRegion r;
    r.name = f[1];
    if (!core::parse_u16(f[2], r.x) || !core::parse_u16(f[3], r.y) ||
        !core::parse_u16(f[4], r.w) || !core::parse_u16(f[5], r.h))
        return fail(err, line, "the rectangle takes four whole numbers of pixels");
    if (r.w == 0 || r.h == 0)
        return fail(err, line, "region '" + r.name + "' has a zero side and draws nothing");
    // Сумма считается в 32 битах: `x + w` двух шестнадцатибитных чисел переполняет `uint16_t`, и
    // регион, свисающий за край страницы РОВНО настолько, прошёл бы проверку обёрнутой суммой.
    if (static_cast<uint32_t>(r.x) + r.w > out.page_width ||
        static_cast<uint32_t>(r.y) + r.h > out.page_height)
        return fail(err, line, "region '" + r.name + "' hangs off the page");
    if (!core::parse_fix(f[6], r.pivot.x) || !core::parse_fix(f[7], r.pivot.y))
        return fail(err, line, "the pivot takes two decimal numbers of pixels");
    out.regions.push_back(std::move(r));
    return true;
}

} // namespace

bool parse_atlas(const std::string& text, ParsedAtlas& out, AtlasBakeError& err) {
    out = ParsedAtlas{};
    int line = 0;
    // Номер строки для отказа «на конце файла» берётся у последней СОДЕРЖАТЕЛЬНОЙ строки, а не у
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
        const std::string body = core::trim(raw);
        if (body.empty()) continue;
        content_line = line;

        const std::vector<std::string> f = core::split_fields(body);
        if (f[0] == "atlas") {
            if (!parse_page(out, f, line, err)) return false;
            continue;
        }
        // Порядок проверок несущий: «регион до размера страницы» обязано быть отдельным отказом, а
        // не «регион свисает за край страницы 0x0», — второе отправляет читателя чинить не ту
        // строку.
        if (out.page_width == 0)
            return fail(err, line, "a line before the 'atlas' line: the page size is not known yet");
        if (f[0] == "region") {
            if (!parse_region(out, f, line, err)) return false;
            continue;
        }
        return fail(err, line, "unknown key '" + f[0] + "'");
    }
    const int end_line = content_line > 0 ? content_line : line;
    if (out.page_width == 0) return fail(err, end_line, "the source declares no atlas page");
    // Пустая нарезка — находка, а не законный «ноль регионов»: таблица без регионов читается без
    // ошибки и означает «атласа нет», то есть отдаёт отладку в рантайм.
    if (out.regions.empty()) return fail(err, end_line, "the source declares no region");
    return true;
}

} // namespace framework::graphics
