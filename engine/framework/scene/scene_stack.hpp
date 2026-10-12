#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "schedule.hpp"

namespace framework::scene {

constexpr std::size_t SCENE_STACK_CAPACITY = 4;
constexpr std::size_t SCENE_QUEUE_CAPACITY = 8;

struct Scene {
    const Schedule* schedule = nullptr;
    void* user = nullptr;
    bool rollback = false;
};

enum class SceneResult : uint32_t {
    Ok = 0,
    BadScene = 1,
    Overflow = 2,
    Empty = 3,
    QueueFull = 4,
    // Команда из системы rollback-сцены в `run` или любой сцены в `resim`: стек вне снапшота, и ресим
    // подал бы её заново, а команда предсказанного тика применилась бы и после отменившей его
    // коррекции. Её подаёт код игры по подтверждённому тику, вне `run`/`resim`.
    Rollback = 5,
};

const char* scene_reason(SceneResult r);

class SceneStack {
public:
    SceneResult push(const Scene& s);
    SceneResult pop();
    SceneResult replace(const Scene& s);

    // Изнутри расписания, которое исполняют `run`/`resim`, вложенные `apply`, `run` и `resim` ничего не
    // делают: очередь подтверждённого тика посреди ресима не применяется.
    std::size_t apply();
    void run(const Tick& t);
    // Ресим верхней сцены сессией отката: расписание без `apply`, команды стека из систем — Rollback.
    void resim(const Tick& t);

    std::size_t depth() const { return depth_; }
    std::size_t pending() const { return queued_; }
    const Scene* top() const;
    const Scene* at(std::size_t i) const { return i < depth_ ? &scenes_[i] : nullptr; }

private:
    enum class Op : uint8_t { Push, Pop, Replace };

    struct Command {
        Op op = Op::Push;
        Scene scene;
    };

    SceneResult enqueue(Op op, const Scene& s, std::size_t next_depth);
    void execute(const Scene& s, const Tick& t, bool rollback);

    std::array<Scene, SCENE_STACK_CAPACITY> scenes_{};
    std::array<Command, SCENE_QUEUE_CAPACITY> queue_{};
    std::size_t depth_ = 0;
    std::size_t queued_ = 0;
    std::size_t projected_ = 0;
    bool running_ = false;
    bool in_rollback_ = false;
};

} // namespace framework::scene
