#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "section_bake.hpp"
#include "section_format.hpp"
#include "touch_format.hpp"
#include "touch_layout.hpp"

namespace {

int fails = 0;
int cases = 0;

void check(bool ok, const char* what) {
    ++cases;
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::touch;

const char* const SHOOTER =
    "# stick on the left half, fire under the right thumb\n"
    "stick | 48\n"
    "button | a | bottom-right | 72 | 64 | 40\n"
    "button | b | top-left | 24.5 | 16 | 20\n";

std::vector<uint8_t> bake(const char* text) {
    Layout layout;
    std::vector<uint8_t> out;
    std::string error;
    if (!parse_layout(text, "touch.txt", layout, error) || !bake_layout(layout, out, error))
        std::printf("    bake: %s\n", error.c_str());
    return out;
}

void test_round_trip() {
    const std::vector<uint8_t> bytes = bake(SHOOTER);
    Layout l;
    check(read_layout(bytes.data(), bytes.size(), l), "the shooter layout bakes and reads");
    check(l.stick_radius == fix32::from_int(48) && l.button_count == 2, "stick radius and button count");
    const ButtonSpec& a = l.buttons[0];
    check(a.button == 0 && a.anchor == Anchor::BottomRight && a.dx == fix32::from_int(72) &&
              a.dy == fix32::from_int(64) && a.radius == fix32::from_int(40),
          "button a keeps its corner, offset and radius");
    const ButtonSpec& b = l.buttons[1];
    check(b.button == 1 && b.anchor == Anchor::TopLeft && b.dx == fix32::from_raw(24 * 65536 + 32768),
          "button b keeps a fractional offset exactly");
    Layout bare;
    const std::vector<uint8_t> stick_only = bake("stick | 30\n");
    check(read_layout(stick_only.data(), stick_only.size(), bare) && bare.button_count == 0,
          "a layout of the stick alone is a layout");
}

void refused(const char* text, const char* expected, const char* what) {
    Layout l;
    std::string error;
    const bool ok = parse_layout(text, "touch.txt", l, error);
    const bool named = error.find(expected) != std::string::npos;
    if (ok || !named) std::printf("    got: %s\n", ok ? "accepted" : error.c_str());
    check(!ok && named, what);
}

void test_source_refusals() {
    refused("\xEF\xBB\xBF" "stick | 40\n", "touch.txt:1: a byte order mark", "BOM");
    refused("", "touch.txt: no stick line", "an empty file");
    refused("button | a | bottom-right | 1 | 1 | 1\n", "touch.txt: no stick line", "buttons without a stick");
    refused("stick | 40\nstick | 40\n", "touch.txt:2: a second stick line", "two sticks");
    refused("stick\n", "touch.txt:1: a stick line is", "a stick without a radius");
    refused("stick | 0\n", "stick radius is outside", "a zero stick radius");
    refused("stick | 257\n", "stick radius is outside", "a stick radius past the ceiling");
    refused("stick | wide\n", "stick radius 'wide' is not a number", "a word for a radius");
    refused("stick | 40\nbutton | f | bottom-right | 1 | 1 | 1\n", "touch.txt:2: button 'f' is not one of",
            "a sixth button");
    refused("stick | 40\nbutton | a | middle | 1 | 1 | 1\n", "anchor 'middle' is not", "an unknown corner");
    refused("stick | 40\nbutton | a | top-left | 1 | 1\n", "a button line is", "a button with five fields");
    refused("stick | 40\nbutton | a | top-left | 1 | x | 1\n", "has a field that is not a number", "a word offset");
    refused("stick | 40\nbutton | a | top-left | -1 | 1 | 1\n", "offset is outside", "a negative offset");
    refused("stick | 40\nbutton | a | top-left | 1 | 2049 | 1\n", "offset is outside", "an offset past any screen");
    refused("stick | 40\nbutton | a | top-left | 1 | 1 | 0\n", "radius is outside", "a zero button radius");
    refused("stick | 40\nbutton | a | top-left | 1 | 1 | 1\nbutton | a | top-right | 1 | 1 | 1\n",
            "touch.txt:3: button 'a' is placed twice", "the same button twice");
    refused("stick | 40\npad | a\n", "unknown line kind 'pad'", "a foreign line kind");
}

void patched(const std::vector<uint8_t>& bytes, std::size_t at, uint32_t value, const char* what) {
    std::vector<uint8_t> bad = bytes;
    std::memcpy(bad.data() + at, &value, sizeof(value));
    Layout l;
    check(!read_layout(bad.data(), bad.size(), l) && l.button_count == 0, what);
}

void test_reader_refusals() {
    const std::vector<uint8_t> bytes = bake(SHOOTER);
    core::SectionHeader head{};
    std::memcpy(&head, bytes.data(), sizeof(head));
    const std::size_t stick = head.rows_offset;
    const std::size_t button = stick + sizeof(TouchRow);
    patched(bytes, offsetof(core::SectionHeader, count), 0, "a layout of no row");
    patched(bytes, offsetof(core::SectionHeader, version), TOUCH_VERSION + 1, "a foreign version");
    patched(bytes, stick + offsetof(TouchRow, kind), 1, "a button where the stick row stands");
    patched(bytes, stick + offsetof(TouchRow, button), 1, "a stick row with a button number");
    patched(bytes, stick + offsetof(TouchRow, anchor), 1, "a stick row with a corner");
    patched(bytes, stick + offsetof(TouchRow, dx_raw), 1, "a stick row with an offset");
    patched(bytes, stick + offsetof(TouchRow, dy_raw), 1, "a stick row with a vertical offset");
    patched(bytes, stick + offsetof(TouchRow, radius_raw), 0, "a stick of no radius");
    patched(bytes, button + offsetof(TouchRow, kind), 0, "a second stick row");
    patched(bytes, button + offsetof(TouchRow, button), 5, "a button past e");
    patched(bytes, button + offsetof(TouchRow, button), 1, "button a turned into a second b");
    patched(bytes, button + offsetof(TouchRow, anchor), 4, "a corner past top-right");
    patched(bytes, button + offsetof(TouchRow, dy_raw), 0x80000000u, "a negative offset");
    patched(bytes, button + offsetof(TouchRow, radius_raw), 257u << 16, "a radius past the ceiling");
    Layout l;
    check(!read_layout(bytes.data(), bytes.size() - 1, l), "a cut section");
    check(!read_layout(bytes.data(), 0, l), "an empty section");
}

std::vector<uint8_t> built(uint32_t buttons, const char* text) {
    std::vector<TouchRow> rows{{static_cast<uint32_t>(RowKind::Stick), 0, 0, 0, 0, 40 << 16}};
    for (uint32_t i = 0; i < buttons; ++i)
        rows.push_back({static_cast<uint32_t>(RowKind::Button), i % BUTTONS, 0, 1 << 16, 1 << 16, 1 << 16});
    core::SectionBuilder builder(rows.size() * sizeof(TouchRow));
    if (text != nullptr) builder.text(text);
    std::vector<uint8_t> out;
    std::string error;
    builder.finish(TOUCH_MAGIC, TOUCH_VERSION, static_cast<uint32_t>(rows.size()), rows.data(), out, error);
    return out;
}

// Чужой пекарь может собрать секцию, которую свой не собрал бы: строк больше, чем кнопок в раскладке,
// или лишние строки имён.
void test_foreign_sections() {
    Layout l;
    const std::vector<uint8_t> five = built(BUTTONS, nullptr);
    check(read_layout(five.data(), five.size(), l) && l.button_count == BUTTONS, "a stick and five buttons read");
    const std::vector<uint8_t> six = built(BUTTONS + 1, nullptr);
    check(!read_layout(six.data(), six.size(), l) && l.button_count == 0, "a sixth button row is refused");
    const std::vector<uint8_t> named = built(1, "a");
    check(!read_layout(named.data(), named.size(), l), "a string blob past its single zero is refused");
}

void test_bake_refusals() {
    std::vector<uint8_t> out;
    std::string error;
    check(!bake_layout(Layout{}, out, error) && error.find("stick radius") != std::string::npos,
          "a layout without a stick radius does not bake");
    Layout two;
    two.stick_radius = fix32::from_int(40);
    two.buttons[0] = {2, Anchor::TopLeft, fix32::from_int(1), fix32::from_int(1), fix32::from_int(1)};
    two.buttons[1] = two.buttons[0];
    two.button_count = 2;
    check(!bake_layout(two, out, error) && error.find("'c' is placed twice") != std::string::npos,
          "the baker checks duplicates without the parser");
}

} // namespace

int main() {
    std::printf("touch layout: touch.txt grammar, LNTL bake and reader\n");
    test_round_trip();
    test_source_refusals();
    test_reader_refusals();
    test_foreign_sections();
    test_bake_refusals();
    std::printf("framework-touch-layout: %s - %d cases\n", fails == 0 ? "PASS" : "FAIL", cases);
    return fails == 0 ? 0 : 1;
}
