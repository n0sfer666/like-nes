#pragma once
#include <cstdint>
#include <cstdio>

#include "depth_floor.hpp"

namespace framework::depth::test {

constexpr fix32 px(int32_t v) { return fix32::from_int(v); }
constexpr fix32 raw(int32_t v) { return fix32::from_raw(v); }

inline int fails = 0;

inline void check(bool ok, const char* what) {
    if (ok) return;
    std::printf("  FAIL: %s\n", what);
    ++fails;
}

struct Expect {
    bool loud;
    bool ok = true;

    void operator()(bool c, const char* what) {
        if (c) return;
        ok = false;
        if (loud) check(false, what);
    }
};

using Slider = void (*)(FloorView, DepthBody&);
using Blocker = bool (*)(FloorView, fix32, fix32);

template <class Rule>
void honest(void (*group)(Rule, Expect&), Rule real) {
    Expect e{true};
    group(real, e);
}

template <class Rule>
void control(void (*group)(Rule, Expect&), Rule broken, const char* what) {
    Expect e{false};
    group(broken, e);
    check(!e.ok, what);
}

enum class Break { Tunnel, OpenEdge, NoBand };

template <Break B>
bool inside(fix32 v, fix32 lo, fix32 hi) {
    if constexpr (B == Break::OpenEdge) return lo < v && v < hi;
    else return !(v < lo) && !(hi < v);
}

template <Break B>
fix32 sweep([[maybe_unused]] fix32 from, fix32 to, [[maybe_unused]] fix32 lo, [[maybe_unused]] fix32 hi) {
    if constexpr (B == Break::Tunnel) {
        return to;
    } else {
        if (from < to && !(lo < from) && lo < to) return lo;
        if (to < from && !(from < hi) && to < hi) return hi;
        return to;
    }
}

template <Break B>
fix32 band(fix32 v, [[maybe_unused]] fix32 lo, [[maybe_unused]] fix32 hi) {
    if constexpr (B == Break::NoBand) return v;
    else return v < lo ? lo : (hi < v ? hi : v);
}

template <Break B>
void broken_slide(FloorView f, DepthBody& b) {
    fix32 x = b.x + b.vx;
    for (const FloorRect& w : f.walls)
        if (inside<B>(b.z, w.z0, w.z1)) x = sweep<B>(b.x, x, w.x0, w.x1);
    x = band<B>(x, f.band.x0, f.band.x1);
    if (!(x == b.x + b.vx)) b.vx = fix32{};
    b.x = x;
    fix32 z = b.z + b.vz;
    for (const FloorRect& w : f.walls)
        if (inside<B>(b.x, w.x0, w.x1)) z = sweep<B>(b.z, z, w.z0, w.z1);
    z = band<B>(z, f.band.z0, f.band.z1);
    if (!(z == b.z + b.vz)) b.vz = fix32{};
    b.z = z;
}

template <Break B>
bool broken_blocked(FloorView f, fix32 x, fix32 z) {
    bool out = !inside<Break::Tunnel>(x, f.band.x0, f.band.x1) || !inside<Break::Tunnel>(z, f.band.z0, f.band.z1);
    if constexpr (B == Break::NoBand) out = false;
    for (const FloorRect& w : f.walls)
        if (inside<B>(x, w.x0, w.x1) && inside<B>(z, w.z0, w.z1)) out = true;
    return out;
}

inline DepthFloor<> walled() {
    DepthFloor<> f;
    f.band = FloorRect{px(0), px(0), px(100), px(50)};
    f.add_wall(FloorRect{px(40), px(10), px(60), px(30)});
    return f;
}

inline DepthBody moved(Slider s, fix32 x, fix32 z, fix32 vx, fix32 vz) {
    const DepthFloor<> f = walled();
    DepthBody b{x, z, fix32{}, vx, vz, fix32{}};
    s(f.view(), b);
    return b;
}

} // namespace framework::depth::test
