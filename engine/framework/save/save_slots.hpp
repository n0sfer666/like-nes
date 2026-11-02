#pragma once
#include "save_backend.hpp"

namespace save {

enum class Status { Ok, Empty, Migrated, MigrateFailed, Future, Corrupt, Conflict, Invalid, Error };
using Migrate = bool (*)(std::vector<uint8_t>& body);

// Backend overloads return a quarantine KEY; the global/local-directory facade returns its path.
// Slot: canonical lowercase a-z/0-9/_/-, <=63 bytes, no DOS device names.
// Caller serializes the whole slot (including all quarantine keys), not just one operation.
// chain[i]: v(i+1)->v(i+2), exactly version-1 non-null callbacks, which must not throw.
// All non-success results clear body. Invalid is a programmer error (no IO); Error is IO failure.
Status load(Backend& backend, const std::string& slot, uint16_t version, std::span<const Migrate> chain,
            std::vector<uint8_t>& body, std::string* quarantine = nullptr);
// Refuses corrupt/future/different-version current records until load handles them. Counter never
// wraps: max readable header (including quarantine, even if body/CRC corrupt) +1, else refusal.
// A write failure after atomic replacement may leave the new complete record visible (writer contract).
bool store(Backend& backend, const std::string& slot, uint16_t version, std::span<const uint8_t> body);

} // namespace save
