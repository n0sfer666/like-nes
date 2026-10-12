#include "rumble_flow.hpp"

#include <cstdio>
#include <string>

#include "rumble.hpp"
#include "rumble_brawl_report.hpp"
#include "rumble_report.hpp"

namespace rumble {

namespace sc = framework::scene;

bool Flow::open() {
    const sc::SceneResult r = stack_.push(menu_.stack_scene());
    if (r != sc::SceneResult::Ok) {
        std::fprintf(stderr, "neon-rumble: menu scene: %s\n", sc::scene_reason(r));
        return false;
    }
    return apply();
}

bool Flow::apply() {
    stack_.apply();
    const void* top = stack_.top() == nullptr ? nullptr : stack_.top()->user;
    if (top == bound_) return false;
    bound_ = top;
    if (top == &menu_) play_.reset();
    return true;
}

// Очередь разбирает `apply` до `run`: иначе стек сменил бы сцену мимо `bound_`, и кадр нарисовал бы
// новую сцену текстурами старой.
void Flow::run(uint32_t tick) {
    if (stack_.pending() != 0) {
        std::fprintf(stderr, "neon-rumble: tick %u: scene commands not applied before the run\n", tick);
        return;
    }
    stack_.run(frame_tick(tick));
}

void Flow::enter() {
    // Вход только с меню на вершине и пустой очередью: прежний уровень тогда уже не в стеке, и замена
    // `play_` его не разрушит из-под стека.
    if (play() != nullptr || stack_.pending() != 0) return;
    std::string error;
    std::unique_ptr<Play> next = enter_level(bundle_, menu_.level.c_str(), menu_.item == MenuItem::Wave, error);
    if (next == nullptr) return refuse(error);
    const sc::SceneResult r = stack_.replace(next->stack_scene());
    if (r != sc::SceneResult::Ok) return refuse(sc::scene_reason(r));
    menu_.error.clear();
    report_level(next->level);
    report_fighters(next->level, next->fighters, next->brawl);
    report_credits(next->level, credits_);
    play_ = std::move(next);
}

void Flow::leave() {
    const sc::SceneResult r = stack_.replace(menu_.stack_scene());
    if (r != sc::SceneResult::Ok) std::fprintf(stderr, "neon-rumble: back to the menu: %s\n", sc::scene_reason(r));
}

void Flow::refuse(const std::string& error) {
    menu_.error = error;
    std::printf("neon-rumble: menu: %s\n", error.c_str());
    std::fflush(stdout);
}

} // namespace rumble
