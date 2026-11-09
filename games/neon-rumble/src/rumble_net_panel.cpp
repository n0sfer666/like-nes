#include "rumble_net_mode.hpp"

#include <cstdio>

namespace rumble {
namespace {

int action(NetAction value) { return static_cast<int>(value); }

const char* difficulty(Difficulty value) {
    switch (value) {
        case Difficulty::Easy: return "easy";
        case Difficulty::Normal: return "normal";
        case Difficulty::Hard: return "hard";
    }
    return "?";
}

std::string invite(net::RoomCode code) {
    char digits[17];
    std::snprintf(digits, sizeof(digits), "%010llx", static_cast<unsigned long long>(code));
    return digits;
}

std::string preview(const std::string& value) {
    return value.size() > 42 ? "..." + value.substr(value.size() - 39) : value;
}

} // namespace

void NetPanel::add(std::string text, float x, float y, float width, int command, bool selected) {
    if (count < cells.size()) cells[count++] = {std::move(text), x, y, width, command, selected};
}

NetPanel NetworkMode::panel() const {
    NetPanel p;
    auto line = [&](std::string text, float y, NetAction command = NetAction::None, bool selected = false) {
        p.add(std::move(text), 0.5f, y, 0.8f, action(command), selected);
    };
    if (fight_) {
        p.add("BACK", 0.12f, 0.07f, 0.16f, action(NetAction::Back));
        p.add("F3 / NET", 0.88f, 0.07f, 0.19f, action(NetAction::Diagnostics));
        if (fight_->phase() == NetworkPhase::Starting) line("Waiting for both fighters", 0.25f);
        if (fight_->phase() == NetworkPhase::Paused) line("Paused. Reconnecting...", 0.25f);
        if (fight_->phase() == NetworkPhase::Solo) line("Solo", 0.25f);
        if (fight_->phase() == NetworkPhase::Desynced) line(status_, 0.25f);
        if (diagnostics_) {
            const uint64_t total = uint64_t{fight_->packets()} + fight_->resends();
            const auto percent = total ? uint64_t{fight_->resends()} * 100 / total : 0;
            const char* route = solo_ ? "solo" : !relay_mode_ || channel_.direct() ? "direct" : "relay";
            line("RTT " + std::to_string(fight_->rtt_ms()) + " ms | loss ~" + std::to_string(percent) +
                 "% | rollback " + std::to_string(fight_->rollbacks()), 0.13f);
            line("Delay " + std::to_string(fight_->delay()) + " | " + route +
                 " | confirmed " + std::to_string(fight_->confirmed()), 0.18f);
        }
        return p;
    }

    line("NEON RUMBLE  /  NETWORK", 0.06f);
    if (!status_.empty()) line(status_, 0.94f);
    if (lookup_) {
        line("Resolving relay address...", 0.25f);
        line("BACK", 0.78f, NetAction::Back);
        return p;
    }
    if (edit_) {
        line(preview(*edit_) + "_", 0.2f);
        for (size_t i = 0; i < NETWORK_TEXT_KEYS.size(); ++i) {
            const float x = 0.10f + 0.08f * static_cast<float>(i % 11);
            const float y = 0.36f + 0.11f * static_cast<float>(i / 11);
            const char c = NETWORK_TEXT_KEYS[i];
            p.add(std::string(1, c), x, y, 0.075f, NETWORK_CHAR_ACTION + c);
        }
        p.add("ERASE", 0.3f, 0.84f, 0.22f, action(NetAction::Erase));
        p.add("DONE", 0.7f, 0.84f, 0.22f, action(NetAction::Done));
        return p;
    }

    if (lobby_) {
        const auto& agreement = lobby_->agreement();
        line(!opened_ ? "Verifying relay address" : ready_ ? "Ready. Waiting for peer." :
             agreement ? "Review match settings" : "Waiting for a peer", 0.2f);
        if (host_ && !relay_mode_ && !discovery_.available()) {
            line("LAN unavailable. Manual join is available.", 0.56f);
        }
        if (host_ && relay_mode_ && channel_.code()) line("Room " + invite(channel_.code()), 0.29f);
        if (agreement) {
            NetworkSettings terms;
            if (decode_network_settings(agreement->data, terms) == net::Refusal::None) {
                line(std::string("level1 | ") + difficulty(terms.difficulty) + " | friendly fire " +
                     (terms.friendly_fire ? "on" : "off"), 0.42f);
                line("Input delay " + std::to_string(agreement->delay) + " ticks", 0.49f);
            }
        }
        if (agreement && !ready_) line("READY", 0.68f, NetAction::Ready, true);
        line("BACK", 0.78f, NetAction::Back);
        return p;
    }

    if (lan_) {
        const auto rooms = discovery_.rooms();
        if (!discovery_.available()) line("LAN unavailable. Use manual join.", 0.25f);
        else if (rooms.empty()) line("Searching the local network...", 0.25f);
        for (size_t i = 0; i < rooms.size(); ++i) {
            const std::string name = preview(rooms[i].name) + " " + platform::net::endpoint_text(rooms[i].endpoint);
            p.add(name, 0.5f, 0.13f + 0.044f * static_cast<float>(i), 0.9f,
                  -static_cast<int>(i) - 1, i == room_);
        }
        line("BACK", 0.85f, NetAction::Back);
        return p;
    }

    const std::array choices{
        std::pair{NetAction::Host, std::string("Host LAN on UDP 7777")},
        std::pair{NetAction::Lan, std::string("Find LAN rooms")},
        std::pair{NetAction::EditAddress, "Address " + preview(address_)},
        std::pair{NetAction::JoinAddress, std::string("Join address")},
        std::pair{NetAction::EditRelay, "Relay " + preview(relay_)},
        std::pair{NetAction::Create, std::string("Create relay room")},
        std::pair{NetAction::EditCode, "Room code " + code_},
        std::pair{NetAction::JoinCode, std::string("Join by code")},
        std::pair{NetAction::Difficulty, std::string("Difficulty ") + difficulty(settings_.difficulty)},
        std::pair{NetAction::Friendly, std::string("Friendly fire ") + (settings_.friendly_fire ? "on" : "off")}
    };
    for (size_t i = 0; i < choices.size(); ++i) {
        line(choices[i].second, 0.17f + 0.067f * static_cast<float>(i), choices[i].first, i == selected_);
    }
    return p;
}

} // namespace rumble
