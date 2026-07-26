#pragma once
#include <memory>
#include <string>

namespace ach {
class Backend;
}

namespace game {

// Плагин-хост в отдельном TU: plugin/registry.hpp объявляет struct EcsSystem, а flecs.h —
// переменную EcsSystem; в одной единице трансляции они несовместимы. Игра видит только
// ach::Backend, заголовки плагинов сюда не протекают.
class BackendHost {
public:
    BackendHost();
    ~BackendHost();
    BackendHost(const BackendHost&) = delete;
    BackendHost& operator=(const BackendHost&) = delete;

    ach::Backend* load(const std::string& plugin_path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace game
