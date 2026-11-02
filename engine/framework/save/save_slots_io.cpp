#include "save_slots_internal.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>

#include "save_format.hpp"
#include "save_io.hpp"

namespace save::detail {
namespace {
bool number(std::string_view text, uint64_t max) {
    if (text.empty() || (text.size() > 1 && text[0] == '0')) return false;
    uint64_t value;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && value <= max;
}
bool belongs(const std::string& key, const std::string& slot) {
    if (key == slot) return true;
    const std::string prefix = slot + ".";
    if (!key.starts_with(prefix)) return false;
    std::string_view tail(key.data() + prefix.size(), key.size() - prefix.size());
    if (tail.ends_with(".bad")) return number(tail.substr(0, tail.size() - 4), UINT64_MAX);
    if (!tail.starts_with('v') || (!tail.ends_with(".bak") && !tail.ends_with(".pre"))) return false;
    tail.remove_prefix(1); tail.remove_suffix(4);
    const size_t dot = tail.find('.');
    return dot != std::string_view::npos && tail.substr(0, dot) != "0" &&
        number(tail.substr(0, dot), UINT16_MAX) && number(tail.substr(dot + 1), UINT64_MAX);
}
uint64_t field(std::span<const uint8_t> bytes, size_t at, size_t count) {
    uint64_t value = 0;
    for (size_t i = 0; i < count; ++i) value |= static_cast<uint64_t>(bytes[at + i]) << (i * 8);
    return value;
}
}

bool valid_slot(const std::string& slot) {
    return slot.size() <= 63 && slot.find('.') == std::string::npos && valid_key(slot);
}
bool valid_load(const std::string& slot, uint16_t version, std::span<const Migrate> chain) {
    return valid_slot(slot) && version != 0 && chain.size() == static_cast<size_t>(version - 1) &&
        std::none_of(chain.begin(), chain.end(), [](Migrate fn) { return fn == nullptr; });
}
bool next_counter(Backend& backend, const std::string& slot, uint64_t& next) {
    std::vector<std::string> keys;
    if (!backend.list(keys)) return false;
    if (std::find(keys.begin(), keys.end(), slot) == keys.end()) keys.push_back(slot);
    uint64_t max = 0;
    std::vector<uint8_t> header;
    for (const auto& key : keys) {
        if (!belongs(key, slot)) continue;
        const auto result = backend.read(key, header, RECORD_HEADER_SIZE);
        if (result == ReadResult::Error) return false;
        if (header.size() != RECORD_HEADER_SIZE || header[0] != 'L' || header[1] != 'N' ||
            header[2] != 'S' || header[3] != 'V') continue;
        // Header is readable, not trusted as valid data. Never reset on a damaged body's CRC.
        max = std::max(max, field(header, 12, 8));
    }
    if (max == std::numeric_limits<uint64_t>::max()) return false;
    next = max + 1;
    return true;
}
bool quarantine(Backend& backend, const std::string& slot, const std::string& version_tag,
                const std::string& extension, std::string& key) {
    std::vector<std::string> keys;
    if (!backend.list(keys)) return false;
    // At most keys.size() existing collisions; a racing external writer may force a safe refusal.
    for (size_t k = 0; k <= keys.size(); ++k) {
        const std::string candidate = slot + version_tag + "." + std::to_string(k) + extension;
        const auto moved = backend.move_new(slot, candidate);
        if (moved == MoveResult::Moved) { key = candidate; return true; }
        if (moved == MoveResult::Error) return false;
    }
    return false;
}
} // namespace save::detail
