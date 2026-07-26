#pragma once
#include "sim.hpp"

inline fix32 sim_dt() { return fix32::from_float(1.0 / 60.0); }

extern "C" {
    void sys_integrate(SimWorld* w);
    void sys_bounce(SimWorld* w);
}
