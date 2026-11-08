#include "rumble_net_fight.hpp"

namespace rumble {
namespace {

constexpr uint32_t DIFF_MAGIC = 0x46464944u;
constexpr size_t NOTICE_BYTES = 4 + 4 + 8;
// A final proof can arrive before the peer's silence timer starts.
constexpr uint64_t NOTICE_MS = 2 * NETWORK_START_MS + NETWORK_RECONNECT_MS + net::LOBBY_RETRY_MS;

} // namespace

void NetworkFight::desync(uint32_t at, uint64_t now) {
    if (mismatch_ && *mismatch_ <= at) return;
    mismatch_ = at;
    phase_ = NetworkPhase::Desynced;
    notice_until_ = now + NOTICE_MS;
    next_notice_ = now;
}

void NetworkFight::notify(uint64_t now, Send send, void* user) {
    if (!mismatch_ || now < next_notice_ || now >= notice_until_) return;
    next_notice_ = now + net::LOBBY_RETRY_MS;
    uint8_t inner[NOTICE_BYTES];
    net::Writer w(inner, sizeof(inner));
    w.u32(DIFF_MAGIC);
    w.u32(*mismatch_);
    w.u64(match_hash_);
    net::Datagram packet;
    if (w.ok() && net::wrap_session(key_, inner, packet)) send(user, key_.peer, packet);
}

bool NetworkFight::accept_notice(std::span<const uint8_t> bytes, uint64_t now) {
    if (bytes.size() != NOTICE_BYTES) return false;
    net::Reader r(bytes.data(), bytes.size());
    uint32_t magic = 0, at = 0;
    uint64_t match = 0;
    if (!r.u32(&magic) || magic != DIFF_MAGIC) return false;
    if (r.u32(&at) && r.u64(&match) && match == match_hash_ && replay_at(at)) desync(at, now);
    return true;
}

} // namespace rumble
