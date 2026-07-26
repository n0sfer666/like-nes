#include "tracker.hpp"
#include <algorithm>
#include <limits>

namespace ach {
namespace {

constexpr uint64_t FNV_OFFSET = 1469598103934665603ull;
constexpr uint64_t FNV_PRIME = 1099511628211ull;

uint64_t fnv_bytes(uint64_t h, const void* p, std::size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (std::size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= FNV_PRIME;
    }
    return h;
}

uint64_t fnv_u64(uint64_t h, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        h ^= static_cast<uint8_t>(v >> (i * 8));
        h *= FNV_PRIME;
    }
    return h;
}

} // namespace

Tracker::Tracker(const Registry& reg) {
    stat_ids_.reserve(reg.stats().size());
    for (const Stat& s : reg.stats()) stat_ids_.push_back(s.id);
    stat_values_.assign(stat_ids_.size(), 0);

    ach_ids_.reserve(reg.entries().size());
    defs_.reserve(reg.entries().size());
    for (const Entry& e : reg.entries()) {
        ach_ids_.push_back(e.def.id);
        defs_.push_back(e.def);
    }
    ach_flags_.assign(ach_ids_.size(), 0);

    by_stat_.resize(stat_ids_.size());
    for (uint32_t i = 0; i < defs_.size(); ++i) {
        if (defs_[i].stat == 0) continue;
        const std::size_t s = stat_slot(defs_[i].stat);
        if (s != Registry::npos) by_stat_[s].push_back(i);
    }
    events_.reserve(ach_ids_.size());
}

std::size_t Tracker::stat_slot(Id stat) const {
    auto at = std::lower_bound(stat_ids_.begin(), stat_ids_.end(), stat);
    if (at == stat_ids_.end() || *at != stat) return Registry::npos;
    return static_cast<std::size_t>(at - stat_ids_.begin());
}

std::size_t Tracker::ach_slot(Id ach) const {
    auto at = std::lower_bound(ach_ids_.begin(), ach_ids_.end(), ach);
    if (at == ach_ids_.end() || *at != ach) return Registry::npos;
    return static_cast<std::size_t>(at - ach_ids_.begin());
}

void Tracker::set_stat(Id stat, uint64_t value) {
    const std::size_t s = stat_slot(stat);
    if (s == Registry::npos || stat_values_[s] == value) return;
    stat_values_[s] = value;
    for (uint32_t i : by_stat_[s]) evaluate(i, value);
}

void Tracker::add_stat(Id stat, uint64_t delta) {
    const std::size_t s = stat_slot(stat);
    if (s == Registry::npos) return;
    const uint64_t cur = stat_values_[s];
    const uint64_t room = std::numeric_limits<uint64_t>::max() - cur;
    set_stat(stat, delta > room ? std::numeric_limits<uint64_t>::max() : cur + delta);
}

void Tracker::unlock(Id ach) {
    const std::size_t a = ach_slot(ach);
    if (a != Registry::npos) emit(a);
}

void Tracker::evaluate(std::size_t def_index, uint64_t value) {
    if (value >= defs_[def_index].target) emit(def_index);
}

void Tracker::emit(std::size_t slot) {
    if (ach_flags_[slot] != 0) return;
    ach_flags_[slot] = 1;
    ++unlocked_count_;
    event_digest_ = fnv_u64(fnv_u64(event_digest_, ach_ids_[slot]), tick_);
    events_.push_back(Event{ach_ids_[slot], tick_});
}

uint64_t Tracker::stat(Id stat) const {
    const std::size_t s = stat_slot(stat);
    return s == Registry::npos ? 0 : stat_values_[s];
}

bool Tracker::unlocked(Id ach) const {
    const std::size_t a = ach_slot(ach);
    return a != Registry::npos && ach_flags_[a] != 0;
}

void Tracker::drain(std::size_t n) {
    const std::size_t k = std::min(n, events_.size());
    events_.erase(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(k));
}

// Записи вне каталога переживают загрузку: каталог бывает урезанным (нет бандла, стаб-сборка,
// откат версии), а перезапись снимка тем, что удалось разобрать, стирала бы прогресс игрока
// целиком. Их не применяют, но возвращают в следующий snapshot() дословно.
void Tracker::carry_stat(const StatRecord& rec) {
    for (StatRecord& s : carried_.stats) {
        if (s.id == rec.id) {
            s.value = rec.value;
            return;
        }
    }
    carried_.stats.push_back(rec);
}

void Tracker::carry_unlocked(Id id) {
    for (Id v : carried_.unlocked) {
        if (v == id) return;
    }
    carried_.unlocked.push_back(id);
}

void Tracker::snapshot(Snapshot& out) const {
    out.stats.clear();
    out.unlocked.clear();
    out.stats.reserve(stat_ids_.size() + carried_.stats.size());
    for (std::size_t i = 0; i < stat_ids_.size(); ++i) {
        out.stats.push_back(StatRecord{stat_ids_[i], stat_values_[i]});
    }
    out.stats.insert(out.stats.end(), carried_.stats.begin(), carried_.stats.end());
    for (std::size_t i = 0; i < ach_ids_.size(); ++i) {
        if (ach_flags_[i] != 0) out.unlocked.push_back(ach_ids_[i]);
    }
    out.unlocked.insert(out.unlocked.end(), carried_.unlocked.begin(), carried_.unlocked.end());
}

std::size_t Tracker::restore(const Snapshot& snap) {
    std::size_t carried = 0;
    for (Id id : snap.unlocked) {
        const std::size_t a = ach_slot(id);
        if (a == Registry::npos) {
            ++carried;
            carry_unlocked(id);
            continue;
        }
        if (ach_flags_[a] == 0) {
            ach_flags_[a] = 1;
            ++unlocked_count_;
        }
    }
    for (const StatRecord& s : snap.stats) {
        const std::size_t i = stat_slot(s.id);
        if (i == Registry::npos) {
            ++carried;
            carry_stat(s);
            continue;
        }
        stat_values_[i] = s.value;
    }
    for (std::size_t i = 0; i < stat_ids_.size(); ++i) {
        for (uint32_t d : by_stat_[i]) evaluate(d, stat_values_[i]);
    }
    return carried;
}

uint64_t Tracker::progress_hash() const {
    uint64_t h = FNV_OFFSET;
    for (std::size_t i = 0; i < stat_ids_.size(); ++i) {
        h = fnv_u64(fnv_u64(h, stat_ids_[i]), stat_values_[i]);
    }
    for (std::size_t i = 0; i < ach_ids_.size(); ++i) {
        h = fnv_u64(h, ach_ids_[i]);
        h = fnv_bytes(h, &ach_flags_[i], 1);
    }
    return h;
}

uint64_t Tracker::hash() const {
    uint64_t h = fnv_u64(progress_hash(), event_digest_);
    return fnv_u64(h, static_cast<uint64_t>(unlocked_count_));
}

} // namespace ach
