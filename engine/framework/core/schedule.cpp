#include "schedule.hpp"

namespace framework {
namespace {

uint32_t stage_index(Stage s) { return static_cast<uint32_t>(s); }

} // namespace

const char* build_reason(BuildResult r) {
    switch (r) {
    case BuildResult::Ok: return "ok";
    case BuildResult::BadSystem: return "system without a name or a function";
    case BuildResult::DuplicateName: return "duplicate system name";
    case BuildResult::UnknownDependency: return "dependency names no registered system";
    case BuildResult::LaterStageDependency: return "dependency lives in a later stage";
    case BuildResult::Cycle: return "dependency cycle";
    }
    return "unknown";
}

bool Schedule::add(const SystemDesc& d) {
    built_ = false;
    if (d.name == nullptr || d.name[0] == '\0' || d.fn == nullptr) {
        error_system_ = d.name != nullptr ? d.name : "<unnamed>";
        return false;
    }
    for (const Entry& e : systems_)
        if (e.name == d.name) {
            error_system_ = d.name;
            return false;
        }
    Entry e;
    e.desc = d;
    e.name = d.name;
    systems_.push_back(std::move(e));
    return true;
}

BuildResult Schedule::build() {
    built_ = false;
    order_.clear();
    error_system_.clear();

    for (Entry& e : systems_) {
        e.deps.clear();
        for (std::size_t i = 0; i < e.desc.after_count; ++i) {
            const char* dep = e.desc.after[i];
            std::size_t found = systems_.size();
            for (std::size_t j = 0; j < systems_.size(); ++j)
                if (systems_[j].name == dep) { found = j; break; }
            if (found == systems_.size()) {
                error_system_ = e.name;
                return BuildResult::UnknownDependency;
            }
            const uint32_t here = stage_index(e.desc.stage);
            const uint32_t there = stage_index(systems_[found].desc.stage);
            if (there > here) {
                error_system_ = e.name;
                return BuildResult::LaterStageDependency;
            }
            // Зависимость из более ранней стадии удовлетворена самой сеткой стадий — ребром внутри
            // стадии она не становится, иначе топосортировка увидела бы вершину не из своего графа.
            if (there == here) e.deps.push_back(found);
        }
    }

    order_.reserve(systems_.size());
    std::vector<bool> done(systems_.size(), false);
    for (uint32_t stage = 0; stage < STAGE_COUNT; ++stage) {
        for (;;) {
            // Из всех готовых берём лексикографически первую: без этого разрыва ничьих порядок
            // зависел бы от порядка регистрации ровно там, где зависимостей нет, — то есть в
            // большинстве случаев.
            std::size_t pick = systems_.size();
            for (std::size_t i = 0; i < systems_.size(); ++i) {
                if (done[i] || stage_index(systems_[i].desc.stage) != stage) continue;
                bool ready = true;
                for (std::size_t d : systems_[i].deps)
                    if (!done[d]) { ready = false; break; }
                if (!ready) continue;
                if (pick == systems_.size() || systems_[i].name < systems_[pick].name) pick = i;
            }
            if (pick == systems_.size()) break;
            done[pick] = true;
            order_.push_back(pick);
        }
        for (std::size_t i = 0; i < systems_.size(); ++i)
            if (!done[i] && stage_index(systems_[i].desc.stage) == stage) {
                error_system_ = systems_[i].name;
                return BuildResult::Cycle;
            }
        stage_end_[stage] = order_.size();
    }

    built_ = true;
    return BuildResult::Ok;
}

void Schedule::run(const Tick& t) const {
    if (!built_) return;
    for (std::size_t i : order_) systems_[i].desc.fn(systems_[i].desc.user, t);
}

void Schedule::run_stage(Stage s, const Tick& t) const {
    if (!built_) return;
    const uint32_t idx = stage_index(s);
    const std::size_t begin = idx == 0 ? 0 : stage_end_[idx - 1];
    for (std::size_t i = begin; i < stage_end_[idx]; ++i)
        systems_[order_[i]].desc.fn(systems_[order_[i]].desc.user, t);
}

const char* Schedule::name_at(std::size_t i) const {
    return i < order_.size() ? systems_[order_[i]].name.c_str() : "";
}

} // namespace framework
