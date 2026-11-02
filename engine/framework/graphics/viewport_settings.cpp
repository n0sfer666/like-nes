#include "viewport_settings.hpp"

#include "section_bake.hpp"
#include "section_open.hpp"

namespace framework::graphics {
namespace {

constexpr uint8_t MAGIC[4] = {'L', 'N', 'V', 'W'};
constexpr uint32_t VERSION = 1;
struct Row { uint32_t zone_w, zone_h, limit_w, limit_h; };
static_assert(sizeof(Row) == 16, "viewport settings row layout pinned");

bool side(uint32_t v) { return v > 0 && v <= VIEWPORT_SIDE_MAX; }

} // namespace

bool viewport_settings_valid(const ViewportSettings& s) {
    return side(s.zone.w) && side(s.zone.h) && side(s.limit.w) && side(s.limit.h);
}

bool viewport_settings_default(const ViewportSettings& s) {
    return s.zone.w == VIEW_ZONE.w && s.zone.h == VIEW_ZONE.h &&
           s.limit.w == VIEW_LIMIT.w && s.limit.h == VIEW_LIMIT.h;
}

bool read_viewport_settings(const void* data, std::size_t size, ViewportSettings& out) {
    out = ViewportSettings{};
    core::SectionView view;
    if (!core::open_section(data, size, MAGIC, VERSION, sizeof(Row), alignof(Row), view) ||
        view.header->count != 1) return false;
    const Row& row = view.at<Row>(view.header->rows_offset, 1).front();
    const ViewportSettings s{{row.zone_w, row.zone_h}, {row.limit_w, row.limit_h}};
    if (!viewport_settings_valid(s)) return false;
    out = s;
    return true;
}

bool bake_viewport_settings(const ViewportSettings& s, std::vector<uint8_t>& out, std::string& error) {
    out.clear();
    error.clear();
    if (!viewport_settings_valid(s)) {
        error = "viewport zone and limit sides must be between 1 and " + std::to_string(VIEWPORT_SIDE_MAX);
        return false;
    }
    const Row row{s.zone.w, s.zone.h, s.limit.w, s.limit.h};
    core::SectionBuilder builder(sizeof(Row));
    return builder.finish(MAGIC, VERSION, 1, &row, out, error);
}

} // namespace framework::graphics
