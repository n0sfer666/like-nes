#include "rumble_net_mode.hpp"

#include <atomic>
#include <system_error>
#include <thread>
#include <utility>

#include "rumble_net_args.hpp"

namespace rumble {
namespace {

std::atomic_flag resolving;
constexpr uint64_t LOOKUP_TIMEOUT_MS = 10'000;

} // namespace

struct NetworkMode::RelayLookup {
    std::array<platform::net::Endpoint, 8> endpoints;
    std::atomic<bool> complete = false;
    size_t count = 0;
    bool host = false;
    net::RoomCode code = 0;
    uint64_t deadline = 0;
};

void NetworkMode::resolve_room(bool host, net::RoomCode code) {
    status_.clear();
    platform::net::Endpoint endpoint;
    if (platform::net::parse_endpoint(relay_, &endpoint)) {
        start({&endpoint, 1}, host, true, code);
        return;
    }

    auto job = std::make_shared<RelayLookup>();
    job->host = host;
    job->code = code;
    job->deadline = now() + LOOKUP_TIMEOUT_MS;
    if (resolving.test_and_set()) {
        status_ = "Relay DNS is busy. Try again.";
        return;
    }
    try {
        // The worker owns its inputs and result. Back or window shutdown can drop the
        // request without joining a potentially blocked system resolver.
        std::thread([job, address = relay_] {
            job->count = resolve_relay_address(address, job->endpoints);
            job->complete.store(true, std::memory_order_release);
            resolving.clear();
        }).detach();
    } catch (const std::system_error&) {
        resolving.clear();
        status_ = "Relay lookup could not start.";
        return;
    }
    lookup_ = std::move(job);
}

void NetworkMode::finish_lookup(uint64_t clock) {
    if (lookup_->complete.load(std::memory_order_acquire)) {
        auto result = std::exchange(lookup_, {});
        if (!result->count) status_ = "Relay address could not be resolved.";
        else start({result->endpoints.data(), result->count}, result->host, true, result->code);
    } else if (clock >= lookup_->deadline) {
        lookup_.reset();
        status_ = "Relay lookup timed out.";
    }
}

} // namespace rumble
