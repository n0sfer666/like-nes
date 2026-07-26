#include "builtin.hpp"

void sys_integrate(SimWorld* w) {
    const fix32 dt = sim_dt();
    for (int i = 0; i < SimWorld::N; ++i) {
        w->px[i] = w->px[i] + w->vx[i] * dt;
        w->py[i] = w->py[i] + w->vy[i] * dt;
    }
}

void sys_bounce(SimWorld* w) {
    const fix32 bound = fix32::from_int(200);
    const fix32 nbound = -bound;
    for (int i = 0; i < SimWorld::N; ++i) {
        if (bound < w->px[i]) { w->px[i] = bound; w->vx[i] = -w->vx[i]; }
        if (w->px[i] < nbound) { w->px[i] = nbound; w->vx[i] = -w->vx[i]; }
        if (bound < w->py[i]) { w->py[i] = bound; w->vy[i] = -w->vy[i]; }
        if (w->py[i] < nbound) { w->py[i] = nbound; w->vy[i] = -w->vy[i]; }
    }
}
