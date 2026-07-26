#include "state.hpp"
#include <cstring>

namespace ach {
namespace {

constexpr uint64_t FNV_OFFSET = 1469598103934665603ull;
constexpr uint64_t FNV_PRIME = 1099511628211ull;

uint64_t fnv(const uint8_t* p, std::size_t n) {
    uint64_t h = FNV_OFFSET;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= FNV_PRIME;
    }
    return h;
}

void put_u32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

void put_u64(std::vector<uint8_t>& out, uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

uint32_t get_u32(const uint8_t* p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(p[i]) << (i * 8);
    return v;
}

uint64_t get_u64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(p[i]) << (i * 8);
    return v;
}

} // namespace

void encode(const Snapshot& snap, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve(STATE_HEADER_SIZE + snap.stats.size() * STATE_STAT_SIZE +
                snap.unlocked.size() * STATE_ACH_SIZE);
    out.insert(out.end(), STATE_MAGIC, STATE_MAGIC + 4);
    put_u32(out, STATE_VERSION);
    put_u32(out, static_cast<uint32_t>(snap.stats.size()));
    put_u32(out, static_cast<uint32_t>(snap.unlocked.size()));
    put_u64(out, 0);
    for (const StatRecord& s : snap.stats) {
        put_u64(out, s.id);
        put_u64(out, s.value);
    }
    for (Id id : snap.unlocked) put_u64(out, id);

    const uint64_t h = fnv(out.data() + STATE_HEADER_SIZE, out.size() - STATE_HEADER_SIZE);
    for (int i = 0; i < 8; ++i) out[16 + static_cast<std::size_t>(i)] = static_cast<uint8_t>(h >> (i * 8));
}

DecodeResult decode(const uint8_t* data, std::size_t size, Snapshot& out) {
    if (data == nullptr || size < STATE_HEADER_SIZE) return DecodeResult::TooShort;
    if (std::memcmp(data, STATE_MAGIC, 4) != 0) return DecodeResult::BadMagic;
    if (get_u32(data + 4) != STATE_VERSION) return DecodeResult::BadVersion;

    const uint64_t stat_count = get_u32(data + 8);
    const uint64_t ach_count = get_u32(data + 12);
    const uint64_t want = static_cast<uint64_t>(STATE_HEADER_SIZE) +
                          stat_count * STATE_STAT_SIZE + ach_count * STATE_ACH_SIZE;
    if (want != size) return DecodeResult::BadSize;
    if (fnv(data + STATE_HEADER_SIZE, size - STATE_HEADER_SIZE) != get_u64(data + 16)) {
        return DecodeResult::BadHash;
    }

    out.stats.clear();
    out.unlocked.clear();
    const uint8_t* p = data + STATE_HEADER_SIZE;
    for (uint64_t i = 0; i < stat_count; ++i, p += STATE_STAT_SIZE) {
        out.stats.push_back(StatRecord{get_u64(p), get_u64(p + 8)});
    }
    for (uint64_t i = 0; i < ach_count; ++i, p += STATE_ACH_SIZE) {
        out.unlocked.push_back(get_u64(p));
    }
    return DecodeResult::Ok;
}

const char* decode_reason(DecodeResult r) {
    switch (r) {
        case DecodeResult::Ok: return "ok";
        case DecodeResult::TooShort: return "too short";
        case DecodeResult::BadMagic: return "bad magic";
        case DecodeResult::BadVersion: return "bad version";
        case DecodeResult::BadSize: return "size mismatch";
        case DecodeResult::BadHash: return "hash mismatch";
        case DecodeResult::Missing: return "no snapshot yet";
        case DecodeResult::Unreadable: return "snapshot unreadable or over the size cap";
    }
    return "unknown";
}

} // namespace ach
