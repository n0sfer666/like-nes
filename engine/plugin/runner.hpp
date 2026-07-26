#pragma once
#include "registry.hpp"
#include "sim.hpp"

inline uint64_t run_sim(const Registry& reg, int n_ticks, bool* sched_ok) {
    SimWorld w;
    sim_init(w);
    auto sched = reg.schedule(sched_ok);
    for (int t = 0; t < n_ticks; ++t) {
        for (const auto& s : sched) s.fn(&w);
        w.tick++;
    }
    return sim_hash(w);
}
