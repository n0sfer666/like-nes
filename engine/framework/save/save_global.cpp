#include "save.hpp"

#include <cstdio>
#include <memory>
#include <utility>

#include "platform_fs.hpp"
#include "platform_redact.hpp"
#include "save_io.hpp"
#include "save_format.hpp"
#include "save_root.hpp"
#include "save_slots_internal.hpp"

namespace save {
namespace {
std::unique_ptr<LocalBackend> local;
std::string requested_root, active_root;
bool warned = false;
bool ready() {
    if (local && !active_root.empty() && platform::is_dir(active_root)) return true;
    local = std::make_unique<LocalBackend>(requested_root);
    active_root = local->root();
    return !active_root.empty();
}
void failure() {
    if (!warned) std::fprintf(stderr, "[save] persistence unavailable; write refused, data stays in memory\n");
    warned = true;
}
}

bool init(const std::string& app) {
    // Resolve before reset: even a rejected argument may alias dir()'s owned string.
    std::string root = detail::valid_slot(app) ? detail::save_root(app) : std::string{};
    local.reset(); active_root.clear(); requested_root = std::move(root); warned = false;
    if (!ready()) { failure(); return false; }
    return true;
}
const std::string& dir() {
    return active_root;
}
std::string file_path(const std::string& key) {
    return !detail::valid_key(key) || !ready() ? std::string{} : dir() + "/" + key;
}
Status load(const char* slot, uint16_t version, std::span<const Migrate> chain,
            std::vector<uint8_t>& body, std::string* quarantine) {
    body.clear();
    if (quarantine) quarantine->clear();
    if (!slot || !detail::valid_load(slot, version, chain)) return Status::Invalid;
    if (!ready()) { failure(); return Status::Error; }
    std::string key;
    const Status status = load(*local, slot, version, chain, body, &key);
    const std::string path = key.empty() ? std::string{} : dir() + "/" + key;
    if (quarantine) *quarantine = path;
    if (status == Status::Error) failure();
    if (status == Status::Corrupt || status == Status::Future || status == Status::MigrateFailed)
        std::fprintf(stderr, "[save] slot '%s' quarantined: %s\n", slot, platform::redact_home(path).c_str());
    return status;
}
bool store(const char* slot, uint16_t version, std::span<const uint8_t> body) {
    if (!slot || !detail::valid_slot(slot) || version == 0 || body.size() > MAX_BODY_SIZE) return false;
    if (!ready() || !store(*local, slot, version, body)) { failure(); return false; }
    warned = false;
    return true;
}
} // namespace save
