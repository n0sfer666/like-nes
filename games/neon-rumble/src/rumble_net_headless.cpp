#include "rumble_net_headless.hpp"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>

#include "platform_fs.hpp"
#include "platform_io.hpp"
#include "rumble_demo.hpp"
#include "rumble_net_fight.hpp"

namespace rumble {
namespace {

using Clock = std::chrono::steady_clock;
namespace udp = platform::net;

void send(void* user, const udp::Endpoint& to, const net::Datagram& packet) {
    auto& socket = *static_cast<udp::Socket*>(user);
    socket.send_to(to, packet.bytes, packet.size);
}

bool load_bundle(const char* name, platform::MappedFile& file, asset::BundleView& bundle) {
    const auto path = platform::exe_dir() + "/" + name;
    return file.open(path) && bundle.open(file.data(), file.size(), false);
}

const char* phase_name(NetworkPhase phase) {
    switch (phase) {
        case NetworkPhase::Starting: return "starting";
        case NetworkPhase::Playing: return "playing";
        case NetworkPhase::Paused: return "paused";
        case NetworkPhase::Solo: return "solo";
        case NetworkPhase::Desynced: return "desynced";
        case NetworkPhase::Failed: return "failed";
    }
    return "invalid";
}

class HeadlessMatch {
public:
    HeadlessMatch(const asset::BundleView& game, NetworkSettings settings, uint32_t frames)
        : game_(game), lobby_(network_lobby_config(settings)), frames_(frames) {}

    bool open(const udp::Endpoint& endpoint, bool host) {
        udp::Endpoint local;
        local.family = endpoint.family;
        if (!socket_.open(host ? endpoint : local)) return false;
        if (!host && !lobby_.join(endpoint, 0)) return false;
        lobby_.ready();
        return true;
    }

    int run() {
        const auto start = Clock::now();
        const uint64_t deadline = uint64_t{frames_} * 1000 / 60 + 70'000;
        while (true) {
            const uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - start).count());
            if (now >= deadline || !exchange(now)) return 1;
            if (fight_) {
                if (!step(now)) return 1;
                if (done_ && now - *done_ >= 750) return report();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    bool exchange(uint64_t now) {
        uint8_t bytes[udp::MAX_DATAGRAM];
        udp::Endpoint from;
        for (int i = 0; i < 32; ++i) {
            const int n = socket_.recv_from(bytes, sizeof(bytes), &from);
            if (n < 0) return false;
            if (!n) break;
            const auto datagram = std::span<const uint8_t>(bytes, static_cast<size_t>(n));
            lobby_.receive(from, datagram, now);
            if (fight_) fight_->receive(from, datagram, now);
        }
        if (auto reply = lobby_.poll(now)) send(&socket_, reply->to, reply->datagram);
        if (lobby_.phase() == net::LobbyPhase::Rejected) {
            std::fprintf(stderr, "neon-rumble network: lobby refusal %u\n",
                         static_cast<unsigned>(lobby_.refusal()));
            return false;
        }
        if (!fight_ && lobby_.phase() == net::LobbyPhase::Started) {
            const auto& agreement = *lobby_.agreement();
            NetworkSettings terms;
            if (decode_network_settings(agreement.data, terms) != net::Refusal::None) return false;
            std::string error;
            play_ = enter_level(game_, "level1", true, error);
            if (!play_) {
                std::fprintf(stderr, "neon-rumble network: %s\n", error.c_str());
                return false;
            }
            fight_ = std::make_unique<NetworkFight>(*play_, terms, agreement, now);
            std::printf("network lobby started delay %u\n", static_cast<unsigned>(agreement.delay));
            std::fflush(stdout);
        }
        return true;
    }

    bool step(uint64_t now) {
        fight_->pump(now, send, &socket_);
        if (now >= next_tick_ && fight_->tick() < frames_) {
            next_tick_ = now + (fight_->tick() % 3 == 0 ? 16 : 17);
            fight_->advance(demo_p1(fight_->tick() + 5 * fight_->local_seat()), now);
        }
        while (const auto* effect = fight_->confirmed_effect()) {
            if (effect->tick != ++effects_) return false;
        }
        const auto phase = fight_->phase();
        if (phase != phase_) {
            phase_ = phase;
            std::printf("network phase %s tick %u confirmed %u\n", phase_name(phase), fight_->tick(),
                        fight_->confirmed());
            std::fflush(stdout);
        }
        if (phase == NetworkPhase::Desynced || phase == NetworkPhase::Failed) return false;
        if (!done_ && fight_->confirmed() >= frames_) done_ = now;
        return true;
    }

    int report() const {
        const auto hash = fight_->hash_at(frames_);
        if (!hash || effects_ != frames_) return 1;
        std::printf("network result tick %u hash %016llx rollbacks %u reconnects %u effects %u\n",
                    frames_, static_cast<unsigned long long>(*hash), fight_->rollbacks(),
                    fight_->reconnects(), effects_);
        return 0;
    }

    const asset::BundleView& game_;
    net::Lobby lobby_;
    udp::Socket socket_;
    std::unique_ptr<Play> play_;
    std::unique_ptr<NetworkFight> fight_;
    uint32_t frames_;
    uint32_t effects_ = 0;
    NetworkPhase phase_ = NetworkPhase::Starting;
    uint64_t next_tick_ = 0;
    std::optional<uint64_t> done_;
};

} // namespace

int run_network_headless(int argc, char** argv) {
    udp::Endpoint endpoint;
    uint32_t frames = 0;
    if (argc == 5 && std::strcmp(argv[3], "--frames") == 0) {
        const char* end = argv[4] + std::strlen(argv[4]);
        const auto result = std::from_chars(argv[4], end, frames);
        if (result.ec != std::errc{} || result.ptr != end) frames = 0;
    }
    if (frames == 0 || !udp::parse_endpoint(argv[2], &endpoint)) {
        std::fprintf(stderr, "usage: neon_rumble [--net-host|--net-join] ADDRESS:PORT --frames N\n");
        return 2;
    }
    platform::MappedFile game_file, library_file;
    asset::BundleView game, library;
    if (!load_bundle("game.bundle", game_file, game) || !load_bundle("library.bundle", library_file, library)) {
        std::fprintf(stderr, "neon-rumble network: both validated bundles are required\n");
        return 1;
    }
    NetworkSettings settings;
    settings.library_hash = library.header().bundle_hash;
    settings.game_hash = game.header().bundle_hash;
    settings.seed = 27;
    HeadlessMatch match(game, settings, frames);
    return match.open(endpoint, std::strcmp(argv[1], "--net-host") == 0) ? match.run() : 1;
}

} // namespace rumble
