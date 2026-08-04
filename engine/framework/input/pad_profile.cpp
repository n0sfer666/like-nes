#include "pad_profile.hpp"

#include "codes.hpp"

namespace framework::input {
namespace {

namespace c = ::input::code;

struct LabelSet {
    const char* south;
    const char* east;
    const char* west;
    const char* north;
};

const LabelSet LABELS[] = {
    {"A", "B", "X", "Y"},          // Xbox
    {"B", "A", "Y", "X"},          // Nintendo: те же позиции, зеркальные надписи
    {"Cross", "Circle", "Square", "Triangle"},
};

} // namespace

const char* button_label(const PadProfile& p, uint16_t code) {
    const LabelSet& l = LABELS[static_cast<uint8_t>(p.labels) < 3 ? static_cast<uint8_t>(p.labels) : 0];
    switch (code) {
    case c::PadA: return l.south;
    case c::PadB: return l.east;
    case c::PadX: return l.west;
    case c::PadY: return l.north;
    case c::LB:   return "LB";
    case c::RB:   return "RB";
    case c::Back: return "Back";
    case c::Start: return "Start";
    default: return "";
    }
}

} // namespace framework::input
