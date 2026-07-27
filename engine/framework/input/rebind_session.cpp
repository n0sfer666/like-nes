#include "rebind_session.hpp"

namespace framework::input {
namespace {

bool same_source(const ::input::Source& a, const ::input::Source& b) {
    return a.kind == b.kind && a.code == b.code && a.sign == b.sign;
}

} // namespace

bool effective_source(const PresetTable& table, uint32_t preset, const RebindStore& store,
                      uint32_t action, uint32_t which, ::input::Source& out) {
    const char* name = table.action_name(preset, action);
    if (name[0] != '\0' && store.get(name, which, out)) return true;
    return table.action_source(preset, action, which, out);
}

RebindConflict find_conflict(const PresetTable& table, uint32_t preset, const RebindStore& store,
                             const ::input::Source& src, int except_action) {
    RebindConflict c;
    if (src.kind == ::input::SourceKind::None) return c;
    for (uint32_t a = 0; a < table.action_count(preset); ++a) {
        if (static_cast<int>(a) == except_action) continue;
        for (uint32_t w = 0; w < table.action_binding_count(preset, a); ++w) {
            ::input::Source s;
            if (!effective_source(table, preset, store, a, w, s)) continue;
            if (same_source(s, src)) {
                c.action = static_cast<int>(a);
                c.which = w;
                return c;
            }
        }
    }
    return c;
}

void RebindSession::begin(int action, uint32_t which) {
    active_ = action >= 0;
    captured_ = false;
    action_ = active_ ? action : -1;
    which_ = which;
    candidate_ = {};
}

void RebindSession::cancel() {
    active_ = false;
    captured_ = false;
    action_ = -1;
    candidate_ = {};
}

bool RebindSession::feed(const ::input::RawEvent& e) {
    if (!active_ || captured_) return false;
    const ::input::Source s = ::input::capture_source(e);
    if (s.kind == ::input::SourceKind::None) return false;
    candidate_ = s;
    captured_ = true;
    return true;
}

bool RebindSession::commit(const PresetTable& table, uint32_t preset, RebindStore& store,
                           bool force, RebindConflict* conflict) {
    if (conflict != nullptr) *conflict = {};
    if (!active_ || !captured_) return false;
    const char* name = table.action_name(preset, static_cast<uint32_t>(action_));
    if (name[0] == '\0') return false;

    const RebindConflict c = find_conflict(table, preset, store, candidate_, action_);
    if (c.action >= 0) {
        if (conflict != nullptr) *conflict = c;
        if (!force) return false;
        const char* other = table.action_name(preset, static_cast<uint32_t>(c.action));
        store.set(other, c.which, {});
    }
    store.set(name, which_, candidate_);
    cancel();
    return true;
}

} // namespace framework::input
