#include "rumble_coop_script.hpp"

#include "rumble_controls.hpp"

namespace rumble {

namespace {

constexpr uint32_t JOIN = 2;
constexpr uint32_t LEAVE_AT = 6600;
constexpr uint32_t BACK = 6660;

} // namespace

void post_key(input::InputEngine& engine, uint16_t code, bool down, uint32_t t) {
    engine.post(input::RawEvent{down ? input::RawKind::KeyDown : input::RawKind::KeyUp, input::DeviceKind::Keyboard,
                                0, code, 0, t});
}

void CoopScript::keys(input::InputEngine& engine, uint32_t t) const {
    if (t == JOIN || t == JOIN + 1 || t == BACK || t == BACK + 1) post_key(engine, KEY_KP1, t == JOIN || t == BACK, t);
    if (t == LEAVE_AT || t == LEAVE_AT + 1) post_key(engine, KEY_KP_SUBTRACT, t == LEAVE_AT, t);
}

void CoopScript::next(Brawl& brawl, uint32_t t) {
    for (uint32_t p = 0; p < bots.size(); ++p)
        if (brawl.players[p].input.present) brawl.players[p] = bots[p].next(brawl, t);
}

} // namespace rumble
