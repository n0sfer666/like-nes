#include "fixed.hpp"

#include <cstdint>
#include <cstdio>

#include "platform_args.hpp"

// Comments in docs/examples/ are English on purpose: these files exist to be shown to the reader of
// the documentation, whose source of truth is English (spec #19, decision 1). Everywhere else in the
// tree comments are Russian -- see .context/conventions.md.
//
// What this example claims: simulation arithmetic is Q16.16 fixed point, it never traps and never
// wraps, and the same expression yields the same bits on every machine we build for.

// docs:begin(print)
// Only the raw bits are printed, never the double. `to_double` exists for rendering and for reading
// a value in a debugger; comparing simulation results through it would compare the printf of the
// platform's libc, not the engine.
static void show(const char* what, fix32 v) {
    std::printf("%-28s raw 0x%08x\n", what, static_cast<unsigned>(v.raw));
}
// docs:end(print)

int main(int argc, char** argv) {
    platform::Args args(argc, argv);

    // docs:begin(basics)
    // `from_int` is exact. `from_float` rounds to the nearest representable value and is meant for
    // authoring constants, not for the hot path: it is the only door where a double gets in.
    const fix32 three = fix32::from_int(3);
    const fix32 half = fix32::from_float(0.5);
    show("3 * 0.5", three * half);
    show("1 / 3", fix32::from_int(1) / three);
    show("0.1 + 0.2", fix32::from_float(0.1) + fix32::from_float(0.2));
    // docs:end(basics)

    // docs:begin(saturation)
    // Overflow saturates at the ends of the range instead of wrapping, and division by zero
    // saturates too. Both are deliberate: a wrapped coordinate is a character teleporting across the
    // level, and a trap is a crash in the middle of a tick. Neither is silent -- a saturated value
    // stays pinned at the limit, which is visible, while a wrapped one looks like plausible data.
    const fix32 top = fix32::from_raw(INT32_MAX);
    show("saturated add", top + fix32::from_int(1));
    show("division by zero", fix32::from_int(1) / fix32::from_int(0));
    // docs:end(saturation)

    return 0;
}
