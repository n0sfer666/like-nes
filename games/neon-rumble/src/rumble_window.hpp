#pragma once
#include "rumble_credits.hpp"
#include "rumble_flow.hpp"
#include "rumble_hotseat.hpp"

namespace rumble {

// Окно открывается на меню стека `flow`: ↑↓ выбирают пункт, Enter входит в уровень или листает титры,
// Esc из уровня возвращает в меню. `frames` > 0 — столько кадров, и окно закрывается.
int run_window(Flow& flow, Hotseat& hotseat, const Credits& credits, int frames);

} // namespace rumble
