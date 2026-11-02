#include <cmath>
#include <cstdio>

#include "font_bake.hpp"
#include "platform_fs.hpp"
#include "rumble_touch.hpp"
#include "rumble_touch_quads.hpp"

namespace {
namespace gr = framework::graphics;
namespace tl = framework::touch;
int fails;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("  FAIL: %s\n", why); } }
fix32 pt(int n) { return fix32::from_int(n); }

struct Font {
    std::vector<uint8_t> bytes;
    gr::FontTable table;
    gr::FontView view;
    Font() {
        gr::FontSrc f{"ui", 8, 1, 8, 8, {}};
        f.glyphs.push_back({'?', 0, 0, 5, 6});
        for (uint32_t c = 'A'; c <= 'Z'; ++c) f.glyphs.push_back({c, 0, 0, 5, 6});
        std::string why;
        check(gr::bake_fonts({&f, 1}, bytes, why) && table.open(bytes.data(), bytes.size()), "test font opens");
        view = table.find("ui");
    }
};

void geometry() {
    std::string text, why;
    tl::Layout layout;
    check(platform::read_text(RUMBLE_TEST_TOUCH, text) && tl::parse_layout(text, "touch.txt", layout, why),
          "actual game layout parses");
    tl::TouchSource source;
    check(source.set_layout(layout), "source accepts layout");
    source.set_view(pt(844), pt(390), {pt(44), {}, pt(44), pt(21)});
    Font font;
    rumble::TouchQuads ui;
    rumble::Layers layers;
    for (float scale : {1.0f, 2.75f, 3.0f}) {
        rumble::LayerStats st;
        ui.add(source, font.view, scale, layers, st, 6, 7);
        check(st.quads == 62 && st.runs == 10 && st.unknown == 0 && st.dropped == 0,
              "five circles and all 22 label glyphs survive at integer/fractional density");
        const auto quads = layers.quads(st);
        if (quads.size() < 8) continue;
        const float cx = 732 * scale, cy = 301 * scale, radius = 30 * scale;
        for (uint32_t i = 0; i < 8; ++i) {
            const render::Quad& q = quads[i];
            check(std::abs(q.x + q.w / 2 - cx) < .01f, "button center is points times density, not viewport k");
            for (float x : {q.x, q.x + q.w}) for (float y : {q.y, q.y + q.h})
                check((x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius + .01f,
                      "visible circle stays inside its actual touch target");
        }
        check(layers.runs(st)[0].texture == 6 && layers.runs(st)[1].texture == 7,
              "circles use solid texture and labels use the font atlas");
    }
    source.pointer(7, tl::Phase::Down, pt(732), pt(301));
    source.pointer(8, tl::Phase::Down, pt(200), pt(240));
    source.pointer(8, tl::Phase::Move, pt(248), pt(240));
    rumble::LayerStats held;
    ui.add(source, font.view, 3, layers, held, 6, 7);
    check(held.quads == 78 && held.runs == 12 && layers.quads(held)[0].rgba == 0xFFE45CA0u,
          "held button highlights and floating stick adds base and knob");
    source.cancel();
    rumble::LayerStats released;
    ui.add(source, font.view, 3, layers, released, 6, 7);
    check(released.quads == 62 && layers.quads(released)[0].rgba == 0xFFFFFF40u,
          "cancel hides stick and releases button highlight");
}

void visibility() {
    rumble::Touch touch;
    check(touch.visible(false), "no pad shows controls before joining");
    touch.update_pads(1);
    check(!touch.visible(false) && touch.visible(true), "pad hides unclaimed touch, keeps claimed touch");
    touch.reveal();
    check(touch.visible(false), "screen contact reveals controls while a pad is connected");
    touch.update_pads(3);
    check(!touch.visible(false) && touch.visible(true), "a second new pad also hides unclaimed touch");
    touch.reveal();
    touch.update_pads(2);
    check(touch.visible(false), "disconnecting one pad keeps the revealed controls");
    touch.update_pads(0);
    check(touch.visible(false), "pad loss reveals touch");
    touch.update_pads(4);
    check(!touch.visible(false), "new pad connection hides unclaimed controls again");
}

} // namespace

int main() {
    geometry(); visibility();
    std::puts(fails ? "rumble-touch-quads: FAIL" : "rumble-touch-quads: PASS");
    return fails ? 1 : 0;
}
