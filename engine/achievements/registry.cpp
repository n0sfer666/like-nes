#include "registry.hpp"
#include <algorithm>
#include <cstring>

namespace ach {
namespace {

bool empty_key(const char* s) { return s == nullptr || *s == '\0'; }

bool valid(const Def& def) {
    if (def.kind == static_cast<uint32_t>(Kind::Progress)) {
        return def.stat != 0 && def.target > 0;
    }
    if (def.kind != static_cast<uint32_t>(Kind::Boolean)) return false;
    return def.stat == 0 && def.target == 0;
}

} // namespace

Registry::Transaction::Transaction(Registry& reg)
    : reg_(reg), entries_(reg.entries_), stats_(reg.stats_) {}

Registry::Transaction::~Transaction() {
    if (done_) return;
    reg_.entries_ = std::move(entries_);
    reg_.stats_ = std::move(stats_);
}

const char* Registry::intern(const char* s) {
    arena_.emplace_back(s ? s : "");
    return arena_.back().c_str();
}

DefineResult Registry::adopt(const Def& def, const char* key, const char* name, const char* desc) {
    if (empty_key(key) || def.id != hash_key(key) || !valid(def)) return DefineResult::BadSpec;
    if (def.stat != 0 && find_stat(def.stat) == nullptr) return DefineResult::BadSpec;

    auto at = std::lower_bound(entries_.begin(), entries_.end(), def.id,
                               [](const Entry& e, Id id) { return e.def.id < id; });
    if (at != entries_.end() && at->def.id == def.id) return DefineResult::Duplicate;

    Entry e{def, key, name ? name : "", desc ? desc : ""};
    entries_.insert(at, e);
    return DefineResult::Ok;
}

DefineResult Registry::adopt_stat(Id id, const char* key) {
    if (empty_key(key) || id != hash_key(key)) return DefineResult::BadSpec;

    auto at = std::lower_bound(stats_.begin(), stats_.end(), id,
                               [](const Stat& s, Id v) { return s.id < v; });
    if (at != stats_.end() && at->id == id) {
        return std::strcmp(at->key, key) == 0 ? DefineResult::Ok : DefineResult::Duplicate;
    }
    stats_.insert(at, Stat{id, key});
    return DefineResult::Ok;
}

DefineResult Registry::define_stat(const char* key) {
    if (empty_key(key)) return DefineResult::BadSpec;
    const Id id = hash_key(key);
    if (find_stat(id) != nullptr) return adopt_stat(id, key);
    return adopt_stat(id, intern(key));
}

DefineResult Registry::define(const DefSpec& spec) {
    if (empty_key(spec.key)) return DefineResult::BadSpec;

    Def def{};
    def.id = hash_key(spec.key);
    def.kind = static_cast<uint32_t>(spec.kind);
    def.flags = spec.flags;
    if (spec.kind == Kind::Progress) {
        if (empty_key(spec.stat_key)) return DefineResult::BadSpec;
        def.stat = hash_key(spec.stat_key);
        def.target = spec.target;
    } else if (!empty_key(spec.stat_key) || spec.target != 0) {
        return DefineResult::BadSpec;
    }
    if (!valid(def)) return DefineResult::BadSpec;

    if (find(def.id) != nullptr) return DefineResult::Duplicate;

    // Отказ на середине не должен оставлять следа: стат вставляется до adopt, и переживи он отказ —
    // Tracker получил бы лишний слот, а тот входит в progress_hash, то есть отвергнутая регистрация
    // молча сдвигала бы голден прогресса. arena_ подрезается после отката: до него entries_ ещё
    // указывают на интернированные строки.
    const std::size_t mark = arena_.size();
    DefineResult r = DefineResult::Ok;
    {
        Transaction tx(*this);
        if (def.stat != 0) r = define_stat(spec.stat_key);
        if (r == DefineResult::Ok) {
            r = adopt(def, intern(spec.key), intern(spec.name), intern(spec.desc));
        }
        if (r == DefineResult::Ok) tx.commit();
    }
    if (r != DefineResult::Ok) arena_.resize(mark);
    return r;
}

const Entry* Registry::find(Id id) const {
    auto at = std::lower_bound(entries_.begin(), entries_.end(), id,
                               [](const Entry& e, Id v) { return e.def.id < v; });
    return (at != entries_.end() && at->def.id == id) ? &*at : nullptr;
}

const Stat* Registry::find_stat(Id id) const {
    const std::size_t i = stat_index(id);
    return i == npos ? nullptr : &stats_[i];
}

std::size_t Registry::stat_index(Id id) const {
    auto at = std::lower_bound(stats_.begin(), stats_.end(), id,
                               [](const Stat& s, Id v) { return s.id < v; });
    if (at == stats_.end() || at->id != id) return npos;
    return static_cast<std::size_t>(at - stats_.begin());
}

} // namespace ach
