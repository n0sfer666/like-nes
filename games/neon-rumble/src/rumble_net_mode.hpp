#pragma once
#include "platform_net_discovery.hpp"
#include "rumble_flow.hpp"
#include "rumble_hotseat.hpp"
#include "rumble_net_channel.hpp"
#include "rumble_net_dump.hpp"

#include <chrono>

namespace rumble {

enum class NetAction : int {
    None, Host, Lan, EditAddress, JoinAddress, EditRelay, Create, EditCode, JoinCode,
    Difficulty, Friendly, Ready, Back, Diagnostics, Erase, Done
};

struct NetCell {
    std::string text;
    float x = 0.5f;
    float y = 0.5f;
    float width = 0.7f;
    int action = 0;
    bool selected = false;
};

struct NetPanel {
    std::array<NetCell, 64> cells;
    size_t count = 0;
    void add(std::string text, float x, float y, float width, int action, bool selected = false);
};

inline constexpr std::string_view NETWORK_TEXT_KEYS = "0123456789abcdefghijklmnopqrstuvwxyz.:-[]%";
inline constexpr int NETWORK_CHAR_ACTION = 128;

class NetworkMode {
public:
    NetworkMode(Flow& flow, uint64_t library_hash, uint64_t game_hash, bool russian);
    void poll();
    void advance(Hotseat& hotseat);
    void move(int delta);
    void enter();
    void back();
    void diagnostic() { diagnostics_ = !diagnostics_; }
    void type(uint32_t character);
    bool tap(float x, float y);
    NetPanel panel() const;
    bool playing() const { return fight_ != nullptr; }
    bool editing() const { return edit_ != nullptr; }

private:
    void choose(NetAction action);
    void resolve_room(bool host, net::RoomCode code);
    void finish_lookup(uint64_t now);
    void start(std::span<const platform::net::Endpoint> endpoints, bool host, bool relay, net::RoomCode code = 0);
    void reset(std::string status = {});
    void exchange(uint64_t now);
    void open_match(uint64_t now);
    void effects(Hotseat& hotseat);
    void dump(uint64_t now);
    uint64_t now() const;
    std::string lobby_error(net::Refusal reason) const;

    using Clock = std::chrono::steady_clock;
    Flow& flow_;
    Clock::time_point start_ = Clock::now();
    Clock::time_point next_tick_{};
    platform::net::Discovery discovery_;
    NetworkChannel channel_;
    NetworkSettings settings_;
    std::optional<net::Lobby> lobby_;
    std::unique_ptr<NetworkFight> fight_;
    std::string address_ = "127.0.0.1:7777";
    std::string relay_ = "127.0.0.1:7777";
    std::string code_;
    std::string status_;
    NetworkDump dump_;
    struct RelayLookup;
    std::shared_ptr<RelayLookup> lookup_;
    std::string* edit_ = nullptr;
    size_t selected_ = 0;
    size_t room_ = 0;
    bool russian_ = false;
    bool host_ = false;
    bool relay_mode_ = false;
    bool opened_ = false;
    bool ready_ = false;
    bool entered_ = false;
    bool lan_ = false;
    bool diagnostics_ = false;
    bool solo_ = false;
};

} // namespace rumble
