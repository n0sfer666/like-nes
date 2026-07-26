#pragma once
#include <memory>
#include <string>

#include "world.hpp"

namespace ach {
class Registry;
class Tracker;
class BundleSource;
class LocalStore;
class Backend;
class Delivery;
} // namespace ach

namespace game {

struct Toast {
    std::string name;
    uint32_t left = 0;
};

// Наблюдатель за sim: читает GameState ПОСЛЕ тика, sim про достижения не знает.
// Отсутствие бандла/плагина/сейва не ломает игру — прогресс живёт локально.
class Achievements {
public:
    Achievements();
    ~Achievements();

    void init(const std::string& bundle_path, const std::string& save_path,
              const std::string& plugin_path);
    void observe(const GameState& gs);
    void pump();
    void save();
    void autosave();

    const Toast& toast() const { return toast_; }
    std::size_t unlocked_count() const;
    std::size_t defined_count() const;
    bool has_backend() const { return backend_ != nullptr; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Toast toast_;
    ach::Backend* backend_ = nullptr;
};

} // namespace game
