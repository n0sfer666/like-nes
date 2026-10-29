#pragma once
#include "depth_body.hpp"
#include "depth_profile.hpp"

namespace framework::depth {

// Намерение тела belt на тик: оси стика и подскок. Кнопки игры сюда не доходят — `hop` выводит игра,
// и отсутствующему игроку она отдаёт нулевое намерение.
struct DepthIntent {
    fix32 mx, mz;
    bool hop = false;
};

bool grounded(const DepthBody& d);
void drive(DepthBody& d, const DepthIntent& in, const DepthProfile& p);
void fall(DepthBody& d, const DepthProfile& p);

} // namespace framework::depth
