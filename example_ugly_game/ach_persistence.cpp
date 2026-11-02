#include "ach_persistence.hpp"

#include <algorithm>
#include <cstdio>

#include "platform_fs.hpp"
#include "platform_path.hpp"
#include "platform_redact.hpp"
#include "save_backend.hpp"
#include "store.hpp"
#include "tracker.hpp"

namespace game {
namespace {
bool carry_safe(const ach::Tracker& tracker, const ach::Snapshot& old) {
    size_t stats = 0, unlocks = 0;
    for (const auto& stat : old.stats)
        if (!std::binary_search(tracker.stat_ids().begin(), tracker.stat_ids().end(), stat.id) && ++stats > ach::Tracker::MAX_CARRIED) return false;
    for (const auto id : old.unlocked)
        if (!std::binary_search(tracker.ach_ids().begin(), tracker.ach_ids().end(), id) && ++unlocks > ach::Tracker::MAX_CARRIED) return false;
    return true;
}
}
void AchievementStore::rejected(const char* reason) {
    if (!warned_) std::fprintf(stderr, "[game] achievements: %s; original retained, progress stays in memory\n", reason);
    warned_ = true;
}
void AchievementStore::open(const std::string& path, AchievementPath retry_path, ach::Tracker& tracker) {
    path_ = path; retry_path_ = retry_path;
    if (!path_.empty()) (void)attach(tracker, false);
}
bool AchievementStore::preserve_original() {
    if (!platform::is_absolute(path_)) return false;
    const size_t slash = path_.find_last_of("/\\");
    const std::string key = path_.substr(slash + 1);
    save::LocalBackend backend(platform::parent_dir(path_));
    std::vector<std::string> keys;
    if (!backend.list(keys)) return false;
    for (size_t k = 0; k <= keys.size(); ++k) {
        const std::string backup = key + "." + std::to_string(k) + ".pre";
        const auto moved = backend.move_new(key, backup);
        if (moved == save::MoveResult::Error) return false;
        if (moved == save::MoveResult::Moved) {
            std::fprintf(stderr, "[game] achievements: recovery original retained: %s\n",
                         platform::redact_home(platform::parent_dir(path_) + "/" + backup).c_str());
            return true;
        }
    }
    return false;
}
bool AchievementStore::attach(ach::Tracker& tracker, bool late) {
    if (!platform::is_dir(platform::parent_dir(path_))) return false;
    ach::Snapshot old;
    ach::DecodeResult why;
    if (!ach::LocalStore(path_).read(old, &why)) {
        if (why == ach::DecodeResult::Missing) { attached_ = true; return true; }
        rejected(ach::decode_reason(why)); return false;
    }
    if (!carry_safe(tracker, old)) { rejected("snapshot exceeds the carry ceiling"); return false; }
    ach::Snapshot session;
    if (late) {
        tracker.snapshot(session);
        if (!preserve_original()) return false;
        // restore() changes only IDs present in old; absent old stats must contribute zero.
        for (const auto id : tracker.stat_ids()) tracker.set_stat(id, 0);
    }
    tracker.restore(old); // Keeps current unlocks; existing Delivery references stay valid.
    if (late) for (const auto& stat : session.stats) tracker.add_stat(stat.id, stat.value); // Saturating lifetime sum.
    attached_ = true; // Before publication: a failed write must not merge the same old image twice.
    warned_ = false;
    return true;
}
bool AchievementStore::save(ach::Tracker& tracker) {
    if (!attached_) {
        if (retry_path_) path_ = retry_path_();
        if (path_.empty() || !attach(tracker, true)) return false;
    }
    return ach::LocalStore(path_).save(tracker);
}
}
