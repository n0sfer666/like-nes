#include "rumble_net_input.hpp"

#include <bit>

namespace rumble {
namespace {

constexpr uint8_t INPUT_KIND = 1;
constexpr uint16_t INPUT_BUTTONS = br::button::ATTACK | br::button::JUMP | br::button::GRAB |
    br::button::BLOCK | br::button::DODGE | br::button::RUN;
constexpr int32_t UNIT = fix32::from_int(1).raw;

bool valid_command(const PlayerCommand& command) {
    const auto& move = command.input.move;
    return command.input.present && !(command.input.buttons & ~INPUT_BUTTONS) &&
        command.attack <= Attack::Grab && move.move_x.raw >= -UNIT && move.move_x.raw <= UNIT &&
        move.move_z.raw >= -UNIT && move.move_z.raw <= UNIT;
}

void write_command(net::Writer& w, const PlayerCommand& command) {
    const auto& move = command.input.move;
    w.u32(std::bit_cast<uint32_t>(move.move_x.raw));
    w.u32(std::bit_cast<uint32_t>(move.move_z.raw));
    w.u16(command.input.buttons);
    w.u8(static_cast<uint8_t>(command.attack));
    w.u8(static_cast<uint8_t>((move.jump_held ? 1 : 0) |
                              (move.down_held ? 2 : 0) | (move.up_held ? 4 : 0)));
}

bool read_command(net::Reader& r, PlayerCommand& command) {
    uint32_t x = 0, z = 0;
    uint8_t attack = 0, flags = 0;
    if (!r.u32(&x) || !r.u32(&z) || !r.u16(&command.input.buttons) ||
        !r.u8(&attack) || !r.u8(&flags) || flags > 7) return false;
    command.input.move.move_x = fix32::from_raw(std::bit_cast<int32_t>(x));
    command.input.move.move_z = fix32::from_raw(std::bit_cast<int32_t>(z));
    command.input.move.jump_held = (flags & 1) != 0;
    command.input.move.down_held = (flags & 2) != 0;
    command.input.move.up_held = (flags & 4) != 0;
    command.input.present = true;
    command.attack = static_cast<Attack>(attack);
    return valid_command(command);
}

bool valid_header(const NetworkInputPacket& p) {
    return p.probe != 0 && p.count <= p.inputs.size() &&
        p.begin <= UINT32_MAX - p.count && p.hash_count > 0 &&
        p.hash_count <= p.hashes.size() && p.hash_tick <= p.confirmed &&
        p.hash_count - 1u <= p.confirmed - p.hash_tick;
}

} // namespace

size_t encode_network_input(const NetworkInputPacket& p, std::span<uint8_t> bytes) {
    if (!valid_header(p)) return 0;
    net::Writer w(bytes.data(), bytes.size());
    w.u8(INPUT_KIND);
    w.u32(p.begin);
    w.u32(p.confirmed);
    w.u32(p.hash_tick);
    w.u32(p.probe);
    w.u32(p.echo);
    w.u8(p.count);
    w.u8(p.hash_count);
    for (size_t i = 0; i < p.hash_count; ++i) w.u64(p.hashes[i]);
    for (size_t i = 0; i < p.count; ++i) {
        if (!valid_command(p.inputs[i])) return 0;
        write_command(w, p.inputs[i]);
    }
    return w.ok() ? w.size() : 0;
}

bool decode_network_input(std::span<const uint8_t> bytes, NetworkInputPacket& out) {
    NetworkInputPacket p;
    net::Reader r(bytes.data(), bytes.size());
    uint8_t kind = 0;
    if (!r.u8(&kind) || kind != INPUT_KIND || !r.u32(&p.begin) ||
        !r.u32(&p.confirmed) || !r.u32(&p.hash_tick) ||
        !r.u32(&p.probe) || !r.u32(&p.echo) || !r.u8(&p.count) || !r.u8(&p.hash_count) ||
        !valid_header(p)) return false;
    for (size_t i = 0; i < p.hash_count; ++i) {
        if (!r.u64(&p.hashes[i])) return false;
    }
    for (size_t i = 0; i < p.count; ++i) {
        if (!read_command(r, p.inputs[i])) return false;
    }
    if (r.remaining() != 0) return false;
    out = p;
    return true;
}

} // namespace rumble
