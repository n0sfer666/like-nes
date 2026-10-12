#include "scene_stack.hpp"

namespace framework::scene {

namespace {

bool valid(const Scene& s) { return s.schedule != nullptr && s.schedule->built(); }

} // namespace

const char* scene_reason(SceneResult r) {
    switch (r) {
    case SceneResult::Ok: return "ok";
    case SceneResult::BadScene: return "scene without a built schedule";
    case SceneResult::Overflow: return "scene stack is full";
    case SceneResult::Empty: return "scene stack is empty";
    case SceneResult::QueueFull: return "too many scene commands in one tick";
    case SceneResult::Rollback: return "scene command from a rollback scene or a resim";
    }
    return "unknown";
}

SceneResult SceneStack::push(const Scene& s) {
    if (in_rollback_) return SceneResult::Rollback;
    if (!valid(s)) return SceneResult::BadScene;
    if (projected_ == SCENE_STACK_CAPACITY) return SceneResult::Overflow;
    return enqueue(Op::Push, s, projected_ + 1);
}

SceneResult SceneStack::pop() {
    if (in_rollback_) return SceneResult::Rollback;
    if (projected_ == 0) return SceneResult::Empty;
    return enqueue(Op::Pop, Scene{}, projected_ - 1);
}

SceneResult SceneStack::replace(const Scene& s) {
    if (in_rollback_) return SceneResult::Rollback;
    if (!valid(s)) return SceneResult::BadScene;
    if (projected_ == 0) return SceneResult::Empty;
    return enqueue(Op::Replace, s, projected_);
}

SceneResult SceneStack::enqueue(Op op, const Scene& s, std::size_t next_depth) {
    if (queued_ == SCENE_QUEUE_CAPACITY) return SceneResult::QueueFull;
    queue_[queued_++] = Command{op, s};
    projected_ = next_depth;
    return SceneResult::Ok;
}

std::size_t SceneStack::apply() {
    if (running_) return 0;
    for (std::size_t i = 0; i < queued_; ++i) {
        const Command& c = queue_[i];
        switch (c.op) {
        case Op::Push: scenes_[depth_++] = c.scene; break;
        case Op::Pop: scenes_[--depth_] = Scene{}; break;
        case Op::Replace: scenes_[depth_ - 1] = c.scene; break;
        }
    }
    const std::size_t applied = queued_;
    queued_ = 0;
    return applied;
}

void SceneStack::run(const Tick& t) {
    apply();
    if (const Scene* s = top()) execute(*s, t, s->rollback);
}

void SceneStack::resim(const Tick& t) {
    if (const Scene* s = top()) execute(*s, t, true);
}

void SceneStack::execute(const Scene& s, const Tick& t, bool rollback) {
    if (running_) return;
    struct Restore {
        SceneStack& stack;
        ~Restore() { stack.running_ = stack.in_rollback_ = false; }
    } restore{*this};
    running_ = true;
    in_rollback_ = rollback;
    s.schedule->run(t);
}

const Scene* SceneStack::top() const { return depth_ == 0 ? nullptr : &scenes_[depth_ - 1]; }

} // namespace framework::scene
