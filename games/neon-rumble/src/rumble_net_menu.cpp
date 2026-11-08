#include "rumble_net_mode.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>

namespace rumble {
namespace {

namespace udp = platform::net;

constexpr std::array ACTIONS{
    NetAction::Host, NetAction::Lan, NetAction::EditAddress, NetAction::JoinAddress,
    NetAction::EditRelay, NetAction::Create, NetAction::EditCode, NetAction::JoinCode,
    NetAction::Difficulty, NetAction::Friendly
};
} // namespace

void NetworkMode::choose(NetAction action) {
    if (action == NetAction::Back) { back(); return; }
    if (action == NetAction::Diagnostics) { diagnostic(); return; }
    if (action == NetAction::Erase) { type(8); return; }
    if (action == NetAction::Done) { edit_ = nullptr; return; }
    if (action == NetAction::Ready && lobby_ && lobby_->agreement()) {
        ready_ = true;
        lobby_->ready();
        return;
    }
    if (lobby_) return;
    if (action == NetAction::EditAddress) edit_ = &address_;
    if (action == NetAction::EditRelay) edit_ = &relay_;
    if (action == NetAction::EditCode) edit_ = &code_;
    if (action == NetAction::Lan) { lan_ = true; room_ = 0; }
    if (action == NetAction::Friendly) settings_.friendly_fire = !settings_.friendly_fire;
    if (action == NetAction::Difficulty) {
        settings_.difficulty = static_cast<Difficulty>(static_cast<uint8_t>(settings_.difficulty) % 3 + 1);
    }

    std::array<udp::Endpoint, 8> endpoints;
    if (action == NetAction::Host) {
        endpoints[0] = {udp::ADDRESS_ANY, 7777};
        start({endpoints.data(), 1}, true, false);
    } else if (action == NetAction::JoinAddress) {
        if (!udp::parse_endpoint(address_, &endpoints[0])) status_ = "Enter a numeric IPv4 or bracketed IPv6 address.";
        else start({endpoints.data(), 1}, false, false);
    } else if (action == NetAction::Create || action == NetAction::JoinCode) {
        net::RoomCode value = 0;
        if (action == NetAction::JoinCode) {
            const auto* end = code_.data() + code_.size();
            const auto r = std::from_chars(code_.data(), end, value, 16);
            if (r.ec != std::errc{} || r.ptr != end || !value || value > net::MAX_ROOM_CODE) {
                status_ = "Enter the room code from the host.";
                return;
            }
        }
        resolve_room(action == NetAction::Create, value);
    }
}

void NetworkMode::move(int delta) {
    if (edit_ || lobby_ || lookup_) return;
    const auto count = static_cast<int>(lan_ ? discovery_.rooms().size() : ACTIONS.size());
    if (!count) return;
    size_t& index = lan_ ? room_ : selected_;
    index = static_cast<size_t>((static_cast<int>(index % static_cast<size_t>(count)) + delta + count) % count);
}

void NetworkMode::enter() {
    if (edit_) { edit_ = nullptr; return; }
    if (lookup_) return;
    if (fight_) return;
    if (lobby_) { choose(NetAction::Ready); return; }
    if (lan_) {
        const auto rooms = discovery_.rooms();
        if (room_ < rooms.size()) {
            const auto address = rooms[room_].endpoint;
            lan_ = false;
            start({&address, 1}, false, false);
        }
        return;
    }
    choose(ACTIONS[selected_]);
}

void NetworkMode::back() {
    if (edit_) { edit_ = nullptr; return; }
    if (lookup_) { lookup_.reset(); return; }
    if (lan_ && !lobby_) { lan_ = false; return; }
    if (lobby_) reset();
}

void NetworkMode::type(uint32_t character) {
    if (!edit_) return;
    if (character == 8) {
        if (!edit_->empty()) edit_->pop_back();
        return;
    }
    if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
    if (character > 127 || NETWORK_TEXT_KEYS.find(static_cast<char>(character)) == std::string_view::npos) return;
    const size_t limit = edit_ == &code_ ? 10 : 96;
    if (edit_->size() < limit) edit_->push_back(static_cast<char>(character));
}

bool NetworkMode::tap(float x, float y) {
    const auto view = panel();
    for (const auto& cell : std::span(view.cells).first(view.count)) {
        if (!cell.action || std::abs(cell.x - x) > cell.width * 0.5f || std::abs(cell.y - y) > 0.022f) continue;
        if (cell.action >= NETWORK_CHAR_ACTION) type(static_cast<uint32_t>(cell.action - NETWORK_CHAR_ACTION));
        else if (cell.action < 0) { room_ = static_cast<size_t>(-cell.action - 1); enter(); }
        else choose(static_cast<NetAction>(cell.action));
        return true;
    }
    return false;
}

std::string NetworkMode::lobby_error(net::Refusal reason) const {
    constexpr std::array english{
        "", "Different network version", "Different game or library bundle", "Unknown level",
        "Unsupported difficulty", "Unsupported friendly fire", "Invalid match settings",
        "Connection timed out", "Random source unavailable"
    };
    constexpr std::array russian{
        "", "\u0420\u0430\u0437\u043d\u044b\u0435 \u0432\u0435\u0440\u0441\u0438\u0438 \u0441\u0435\u0442\u0438",
        "\u0420\u0430\u0437\u043d\u044b\u0435 \u0431\u0430\u043d\u0434\u043b\u044b",
        "\u041d\u0435\u0438\u0437\u0432\u0435\u0441\u0442\u043d\u044b\u0439 \u0443\u0440\u043e\u0432\u0435\u043d\u044c",
        "\u041d\u0435\u0432\u0435\u0440\u043d\u0430\u044f \u0441\u043b\u043e\u0436\u043d\u043e\u0441\u0442\u044c",
        "\u041d\u0435\u0432\u0435\u0440\u043d\u044b\u0439 \u0443\u0440\u043e\u043d \u043f\u043e \u0441\u0432\u043e\u0438\u043c",
        "\u041d\u0435\u0432\u0435\u0440\u043d\u044b\u0435 \u043f\u0440\u0430\u0432\u0438\u043b\u0430",
        "\u0418\u0441\u0442\u0435\u043a\u043b\u043e \u0432\u0440\u0435\u043c\u044f \u043e\u0436\u0438\u0434\u0430\u043d\u0438\u044f",
        "\u041d\u0435\u0442 \u0438\u0441\u0442\u043e\u0447\u043d\u0438\u043a\u0430 \u0441\u043b\u0443\u0447\u0430\u0439\u043d\u044b\u0445 \u0447\u0438\u0441\u0435\u043b"
    };
    const auto index = static_cast<size_t>(reason);
    return index < english.size() ? (russian_ ? russian[index] : english[index]) : "Lobby refused";
}

} // namespace rumble
