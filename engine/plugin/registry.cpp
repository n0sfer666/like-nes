#include "registry.hpp"
#include <algorithm>
#include <cstdio>

void Registry::add_ecs_system(const std::string& id, std::vector<std::string> after, SimSystemFn fn) {
    for (const auto& s : ecs_) {
        if (s.id == id) {
            std::fprintf(stderr, "[host] duplicate ecs-system id '%s' (owner=%s) rejected (already owned by %s)\n",
                         id.c_str(), current_owner_.c_str(), s.owner.c_str());
            return;
        }
    }
    ecs_.push_back(EcsSystem{id, std::move(after), fn, current_owner_});
}

void Registry::add_asset_codec(const std::string& fourcc, AssetDecodeFn fn) {
    for (const auto& c : codecs_) {
        if (c.fourcc == fourcc) {
            std::fprintf(stderr, "[host] duplicate asset codec '%s' (owner=%s) rejected (already owned by %s)\n",
                         fourcc.c_str(), current_owner_.c_str(), c.owner.c_str());
            return;
        }
    }
    codecs_.push_back(AssetCodec{fourcc, fn, current_owner_});
}

// Единственный учёт БЕЗ отказа по дубликату, и это записано, а не забыто. Три соседа выше и ниже
// (`add_ecs_system`, `add_asset_codec`, `add_backend`) вторую регистрацию того же имени отбивают:
// у них имя — ключ, по которому запись потом ищут (топосорт, выбор кодека, доставка достижения), и
// два владельца одного ключа означают молчаливую подмену. У named-слотов имя — только подпись в
// логе и в панели: проход рендера или шина звука ВЫЗЫВАЮТСЯ все подряд, и второй с тем же id — не
// подмена, а вторая работа. Заводить здесь отказ значит менять счётчики четырёх чужих гейтов
// (`plugin_seam_test`, UI-манифест, hot-reload, `remove_owner`) ради симметрии, а не ради дефекта;
// решение владельца 2026-09-21 — асимметрию оставить и объяснить, а не выровнять заодно.
void Registry::add_named(ExtKind kind, const std::string& id, const std::string& extra, void* fn) {
    if (!ext_in_range(kind) || ext_has_own_storage(kind)) {
        std::fprintf(stderr, "[host] ext kind %d is not a named slot, '%s' (owner=%s) rejected\n",
                     static_cast<int>(kind), id.c_str(), current_owner_.c_str());
        return;
    }
    named_[kind].push_back(NamedExt{id, extra, fn, current_owner_});
}

void Registry::add_backend(const std::string& id, const AchBackendApi* backend) {
    for (const auto& e : backends_) {
        if (e.id == id) {
            std::fprintf(stderr, "[host] duplicate achievement backend '%s' (owner=%s) rejected (already owned by %s)\n",
                         id.c_str(), current_owner_.c_str(), e.owner.c_str());
            return;
        }
    }
    backends_.push_back(BackendExt{id, backend, current_owner_});
}

void Registry::remove_owner(const std::string& owner) {
    auto drop = [&](auto& v) {
        v.erase(std::remove_if(v.begin(), v.end(),
                               [&](const auto& e) { return e.owner == owner; }),
                v.end());
    };
    drop(ecs_);
    drop(codecs_);
    drop(backends_);
    for (auto& v : named_) drop(v);
}

std::vector<ScheduledSystem> Registry::schedule(bool* ok) const {
    std::vector<const EcsSystem*> remaining;
    remaining.reserve(ecs_.size());
    for (const auto& s : ecs_) remaining.push_back(&s);

    auto present = [&](const std::string& id) {
        for (const auto& s : ecs_) if (s.id == id) return true;
        return false;
    };

    std::vector<ScheduledSystem> out;
    std::vector<std::string> placed;
    auto is_placed = [&](const std::string& id) {
        return std::find(placed.begin(), placed.end(), id) != placed.end();
    };

    while (!remaining.empty()) {
        const EcsSystem* pick = nullptr;
        for (const EcsSystem* s : remaining) {
            bool ready = true;
            for (const auto& dep : s->after) {
                if (present(dep) && !is_placed(dep)) { ready = false; break; }
            }
            if (!ready) continue;
            if (!pick || s->id < pick->id) pick = s;
        }
        if (!pick) { if (ok) *ok = false; return out; }
        out.push_back(ScheduledSystem{pick->id, pick->fn, pick->owner});
        placed.push_back(pick->id);
        remaining.erase(std::find(remaining.begin(), remaining.end(), pick));
    }
    if (ok) *ok = true;
    return out;
}

AssetDecodeFn Registry::find_codec(const std::string& fourcc) const {
    for (const auto& c : codecs_) if (c.fourcc == fourcc) return c.fn;
    return nullptr;
}

std::size_t Registry::count(ExtKind kind) const {
    switch (kind) {
        case EXT_ECS_SYSTEM: return ecs_.size();
        case EXT_ASSET_CODEC: return codecs_.size();
        case EXT_ACHIEVEMENT_BACKEND: return backends_.size();
        default: return ext_in_range(kind) ? named_[kind].size() : 0;
    }
}
