#pragma once
#include <string>
namespace ach { class Tracker; }

namespace game {
using AchievementPath = std::string (*)();

// One fresh tracker / one serialized writer. A failed initial attach keeps session deltas in
// that tracker. Recovery backs up the old image, merges it once, then retries publication.
class AchievementStore {
public:
    void open(const std::string& path, AchievementPath retry_path, ach::Tracker& tracker);
    bool enabled() const { return retry_path_ || !path_.empty(); }
    bool save(ach::Tracker& tracker);
private:
    bool attach(ach::Tracker& tracker, bool late);
    bool preserve_original();
    void rejected(const char* reason);
    std::string path_;
    AchievementPath retry_path_ = nullptr;
    bool attached_ = false;
    bool warned_ = false;
};
}
