#include "rumble_net_headless.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "platform_fs.hpp"
#include "platform_io.hpp"
#include "rumble_demo.hpp"
#include "rumble_net_args.hpp"
#include "rumble_net_channel.hpp"
#include "rumble_net_fight.hpp"

namespace rumble {
namespace {

using Clock = std::chrono::steady_clock;
namespace udp = platform::net;

void send_game(void* user, const udp::Endpoint& to, const net::Datagram& packet) {
    static_cast<NetworkChannel*>(user)->send(to, packet, true);
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
    HeadlessMatch(const asset::BundleView& game, NetworkSettings settings, NetworkRun config)
        : game_(game), lobby_(network_lobby_config(settings)), config_(config) {}

    bool open() {
        return channel_.open({config_.endpoints.data(), config_.endpoint_count},
                             config_.host, config_.relay, config_.code, config_.relay_only);
    }

    int run() {
        const auto start = Clock::now();
        const uint64_t deadline = uint64_t{config_.frames} * 1000 / 60 + 70'000;
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
    bool open_lobby(uint64_t now) {
        if (lobby_opened_ || !channel_.ready()) return true;
        if (!config_.host && !lobby_.join(channel_.endpoint(), now)) return false;
        if (config_.host && config_.relay) {
            std::printf("network room %010llx\n", static_cast<unsigned long long>(channel_.code()));
            std::fflush(stdout);
        }
        lobby_.ready();
        lobby_opened_ = true;
        return true;
    }

    bool exchange(uint64_t now) {
        if (phase_ == NetworkPhase::Solo) return true;
        channel_.pump(now);
        if (!open_lobby(now)) return false;
        uint8_t bytes[udp::MAX_DATAGRAM];
        udp::Endpoint from;
        for (int i = 0; i < 32; ++i) {
            const int n = channel_.read(bytes, from);
            if (n < 0) return false;
            if (!n) break;
            const auto datagram = channel_.logical(from, {bytes, static_cast<size_t>(n)});
            if (!open_lobby(now)) return false;
            if (!lobby_opened_ || datagram.empty()) continue;
            lobby_.receive(from, datagram, now);
            if (fight_) fight_->receive(from, datagram, now);
        }
        if (!lobby_opened_) return !channel_.failed();
        if (auto reply = lobby_.poll(now)) channel_.send(reply->to, reply->datagram, false);
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
            channel_.set_match(agreement.key);
            if (config_.relay) std::puts("network route relay");
            std::printf("network lobby started delay %u\n", static_cast<unsigned>(agreement.delay));
            std::fflush(stdout);
        }
        return true;
    }

    bool step(uint64_t now) {
        fight_->pump(now, send_game, &channel_);
        if (config_.relay && channel_.direct() != direct_) {
            direct_ = channel_.direct();
            std::printf("network route %s\n", direct_ ? "direct" : "relay");
            std::fflush(stdout);
        }
        if (now >= next_tick_ && fight_->tick() < config_.frames) {
            next_tick_ = now + (fight_->tick() % 3 == 0 ? 16 : 17);
            fight_->advance(demo_p1(fight_->tick() + 5 * fight_->local_seat()), now);
        }
        while (const auto* effect = fight_->confirmed_effect()) {
            if (effect->tick != ++effects_) return false;
        }
        const auto phase = fight_->phase();
        if (phase != phase_) {
            phase_ = phase;
            if (phase == NetworkPhase::Solo) channel_.close();
            std::printf("network phase %s tick %u confirmed %u\n", phase_name(phase), fight_->tick(),
                        fight_->confirmed());
            std::fflush(stdout);
        }
        if (phase == NetworkPhase::Desynced || phase == NetworkPhase::Failed) return false;
        if (!done_ && fight_->confirmed() >= config_.frames) done_ = now;
        return true;
    }

    int report() const {
        const auto hash = fight_->hash_at(config_.frames);
        if (!hash || effects_ != config_.frames) return 1;
        std::printf("network result tick %u hash %016llx rollbacks %u reconnects %u effects %u\n",
                    config_.frames, static_cast<unsigned long long>(*hash), fight_->rollbacks(),
                    fight_->reconnects(), effects_);
        return 0;
    }

    const asset::BundleView& game_;
    net::Lobby lobby_;
    NetworkChannel channel_;
    NetworkRun config_;
    bool lobby_opened_ = false;
    bool direct_ = false;
    std::unique_ptr<Play> play_;
    std::unique_ptr<NetworkFight> fight_;
    uint32_t effects_ = 0;
    NetworkPhase phase_ = NetworkPhase::Starting;
    uint64_t next_tick_ = 0;
    std::optional<uint64_t> done_;
};

} // namespace

int run_network_headless(int argc, char** argv) {
    NetworkRun run;
    if (!parse_network_run(argc, argv, run)) {
        std::fprintf(stderr, "usage: neon_rumble [--net-host|--net-join|--net-create] ADDRESS:PORT --frames N\n"
                             "       neon_rumble --net-code RELAY:PORT CODE --frames N\n");
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
    HeadlessMatch match(game, settings, run);
    return match.open() ? match.run() : 1;
}

} // namespace rumble
