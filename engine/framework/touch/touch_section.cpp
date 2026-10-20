#include <span>

#include "section_bake.hpp"
#include "section_open.hpp"
#include "touch_format.hpp"
#include "touch_layout.hpp"

namespace framework::touch {

bool bake_layout(const Layout& layout, std::vector<uint8_t>& out, std::string& error) {
    if (!layout_ok(layout, error)) return false;
    std::vector<TouchRow> rows;
    rows.push_back(TouchRow{static_cast<uint32_t>(RowKind::Stick), 0, 0, 0, 0, layout.stick_radius.raw});
    for (uint32_t i = 0; i < layout.button_count; ++i) {
        const ButtonSpec& b = layout.buttons[i];
        rows.push_back(TouchRow{static_cast<uint32_t>(RowKind::Button), b.button, static_cast<uint32_t>(b.anchor),
                                b.dx.raw, b.dy.raw, b.radius.raw});
    }
    core::SectionBuilder builder(rows.size() * sizeof(TouchRow));
    return builder.finish(TOUCH_MAGIC, TOUCH_VERSION, static_cast<uint32_t>(rows.size()), rows.data(), out, error);
}

bool read_layout(const void* data, std::size_t size, Layout& out) {
    out = Layout{};
    core::SectionView v;
    if (!core::open_section(data, size, TOUCH_MAGIC, TOUCH_VERSION, sizeof(TouchRow), alignof(TouchRow), v))
        return false;
    const uint32_t count = v.header->count;
    if (count == 0 || count > BUTTONS + 1 || v.rows_end != v.header->strings_offset || v.strings_size != 1)
        return false;
    const std::span<const TouchRow> rows = v.at<TouchRow>(v.header->rows_offset, count);
    const TouchRow& s = rows[0];
    if (s.kind != static_cast<uint32_t>(RowKind::Stick) || s.button != 0 || s.anchor != 0 || s.dx_raw != 0 ||
        s.dy_raw != 0)
        return false;
    Layout l;
    l.stick_radius = fix32::from_raw(s.radius_raw);
    for (const TouchRow& r : rows.subspan(1)) {
        if (r.kind != static_cast<uint32_t>(RowKind::Button)) return false;
        l.buttons[l.button_count++] = ButtonSpec{r.button, static_cast<Anchor>(r.anchor), fix32::from_raw(r.dx_raw),
                                                 fix32::from_raw(r.dy_raw), fix32::from_raw(r.radius_raw)};
    }
    std::string why;
    if (!layout_ok(l, why)) return false;
    out = l;
    return true;
}

} // namespace framework::touch
