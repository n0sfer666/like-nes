#include "rumble_menu.hpp"

#include <cstdio>

namespace rumble {

namespace {

void count_menu_tick(void* user, const framework::Tick&) { ++static_cast<Menu*>(user)->ticks; }

} // namespace

bool Menu::init(const char* level_name, bool waves) {
    level = level_name;
    item = waves ? MenuItem::Wave : MenuItem::Level;
    framework::SystemDesc tick;
    tick.name = "count_menu_tick";
    tick.stage = framework::Stage::Sim;
    tick.fn = count_menu_tick;
    tick.user = this;
    if (!schedule.add(tick)) return false;
    const framework::BuildResult r = schedule.build();
    if (r != framework::BuildResult::Ok) {
        std::fprintf(stderr, "neon-rumble: menu schedule build failed: %s\n", framework::build_reason(r));
        return false;
    }
    return true;
}

void Menu::move(int step) {
    const auto n = static_cast<int>(MENU_ITEMS);
    item = static_cast<MenuItem>((static_cast<int>(item) + step % n + n) % n);
}

std::string Menu::label(MenuItem i) const {
    switch (i) {
    case MenuItem::Level: return "level " + level;
    case MenuItem::Wave: return "wave " + level;
    case MenuItem::Credits: break;
    }
    return "credits";
}

} // namespace rumble
