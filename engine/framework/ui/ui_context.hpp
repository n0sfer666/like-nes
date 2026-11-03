#pragma once
#include <array>
#include <optional>
#include <span>

#include "font_read.hpp"
#include "quad_types.hpp"
#include "strings.hpp"
#include "ui_input.hpp"

namespace framework::ui {
using UiId = uint64_t;
UiId ui_id(UiId parent, std::string_view key, uint32_t index = 0);
struct UiRect { int32_t x = 0, y = 0, w = 0, h = 0; };
struct UiView {
    uint32_t width = 512, height = 288, scale = 1;
    int32_t x = 0, y = 0;
    bool operator==(const UiView&) const = default;
};
struct UiTheme {
    uint32_t normal = 0xff302820u, focused = 0xff906040u, disabled = 0xff181818u;
    uint32_t text = 0xffffffffu, muted = 0xff808080u, accent = 0xffd0b070u;
    uint32_t padding = 3;
};
struct UiStats {
    uint32_t widgets = 0, dropped_widgets = 0, duplicate_ids = 0, invalid_widgets = 0;
    uint32_t quads = 0, dropped_quads = 0, clipped_glyphs = 0, unknown_glyphs = 0, missing_labels = 0;
};
struct UiOutput {
    std::span<const render::Quad> quads;
    std::span<const render::QuadRun> runs;
    UiStats stats;
    std::optional<UiId> focus;
    bool back = false;
};
struct UiListItem { std::string_view key, label_key; bool enabled = true; };
inline constexpr uint32_t UI_WIDGETS = 128, UI_QUADS = 2048, UI_RUNS = 512, UI_TEXT_GLYPHS = 256;
inline constexpr size_t UI_FIELD_BYTES = 1025;

// Все буферы фиксированы. Строки/шрифт заимствуют неизменные buffers бандла до следующего init.
// solid_texture — белая текстура1x1, font_texture — атлас шрифта из strings.font_name().
class Context {
public:
    bool init(const Strings& strings, const graphics::FontTable& fonts, uint32_t font_texture,
              uint32_t solid_texture, UiTheme theme = {});
    void set_language(Language language);
    bool begin(const UiInput& input, UiView view = {});
    bool button(UiId id, std::string_view label_key, UiRect rect, bool enabled = true);
    bool slider(UiId id, std::string_view label_key, UiRect rect, float& value,
                float minimum, float maximum, float step, bool enabled = true);
    bool field(UiId id, std::string_view label_key, UiRect rect, std::span<char> buffer, bool enabled = true);
    std::optional<uint32_t> list(UiId parent, std::span<const UiListItem> items, UiRect rect, uint32_t row_height);
    UiOutput end(); // view до следующего begin/init или уничтожения Context
    std::optional<UiId> focus() const { return focus_; }
private:
    struct Widget { UiId id = 0; UiRect rect; bool enabled = false; uint32_t panel_quad = UI_QUADS; };
    struct Pointer { intptr_t id = 0; std::optional<UiId> target; float x = 0, y = 0; bool held = false, released = false; };
    static bool valid_rect(UiRect rect);
    static bool contains(UiRect rect, float x, float y);
    static UiRect intersection(UiRect a, UiRect b);
    Widget* register_widget(UiId id, UiRect rect, bool enabled);
    bool finish_widget(Widget& widget, uint32_t dropped_before);
    bool was_visible(UiId id) const;
    bool activated(UiId id);
    bool dragging(UiId id, float& x) const;
    void pointer_events();
    std::string_view label(std::string_view key);
    void panel(UiId id, UiRect rect, bool enabled);
    void fill(UiRect rect, uint32_t color);
    void text(std::string_view value, UiRect rect, uint32_t color);
    void quad(render::Quad value, uint32_t texture);
    Strings strings_;
    graphics::FontView font_;
    UiTheme theme_;
    UiView view_;
    Language language_ = Language::En;
    uint32_t font_texture_ = 0, solid_texture_ = 0;
    std::array<Widget, UI_WIDGETS> previous_{}, current_{};
    std::array<render::Quad, UI_QUADS> quads_{};
    std::array<render::QuadRun, UI_RUNS> runs_{};
    uint32_t previous_count_ = 0, current_count_ = 0, quad_count_ = 0, run_count_ = 0;
    UiStats stats_;
    UiInput input_;
    Pointer pointer_;
    std::optional<UiId> focus_, click_;
    bool ready_ = false, building_ = false, consumed_ = false;
};
}
