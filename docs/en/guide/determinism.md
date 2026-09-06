# Determinism and fixed-point arithmetic

English · [Русский](../../ru/guide/determinism.md)

The engine promises that the same inputs produce the same simulation on every machine it builds
for. Replays, the network model and half of the project's own gates rest on that promise, so it is
worth being precise about what carries it — and what would break it.

Everything shown below is taken from [`docs/examples/fixed_point.cpp`](../../examples/fixed_point.cpp),
which is built and run by CI on all three operating systems.

## Simulation arithmetic is Q16.16, not floating point

`fix32` is a 32-bit signed fixed-point number with 16 fractional bits. Its range is roughly
±32768 and its step is 1/65536. Every value that reaches the simulation hash is one of these.

Floating point is not banned out of taste. It is banned because the same expression is allowed to
produce different results on different machines: x87 keeps 80-bit intermediates, fused multiply-add
rounds once where two operations would round twice, and compilers reassociate freely under
optimization. Any of those turns "the same replay" into "almost the same replay", which is the same
thing as a broken one.

<!-- snippet: docs/examples/fixed_point.cpp#basics -->
```cpp
// `from_int` is exact. `from_float` rounds to the nearest representable value and is meant for
// authoring constants, not for the hot path: it is the only door where a double gets in.
const fix32 three = fix32::from_int(3);
const fix32 half = fix32::from_float(0.5);
show("3 * 0.5", three * half);
show("1 / 3", fix32::from_int(1) / three);
show("0.1 + 0.2", fix32::from_float(0.1) + fix32::from_float(0.2));
```
<!-- /snippet -->

Note what is printed, and what is not:

<!-- snippet: docs/examples/fixed_point.cpp#print -->
```cpp
// Only the raw bits are printed, never the double. `to_double` exists for rendering and for reading
// a value in a debugger; comparing simulation results through it would compare the printf of the
// platform's libc, not the engine.
static void show(const char* what, fix32 v) {
    std::printf("%-28s raw 0x%08x\n", what, static_cast<unsigned>(v.raw));
}
```
<!-- /snippet -->

## Out of range saturates; it never wraps and never traps

<!-- snippet: docs/examples/fixed_point.cpp#saturation -->
```cpp
// Overflow saturates at the ends of the range instead of wrapping, and division by zero
// saturates too. Both are deliberate: a wrapped coordinate is a character teleporting across the
// level, and a trap is a crash in the middle of a tick. Neither is silent -- a saturated value
// stays pinned at the limit, which is visible, while a wrapped one looks like plausible data.
const fix32 top = fix32::from_raw(INT32_MAX);
show("saturated add", top + fix32::from_int(1));
show("division by zero", fix32::from_int(1) / fix32::from_int(0));
```
<!-- /snippet -->

Saturation is a design decision with a cost: a value pinned at the limit is wrong, just visibly so.
Subsystems that can define their own ceilings do, and they clamp well below saturation — a character
profile whose acceleration sits past the ceiling is not "very lively", it arrives at a speed computed
from a saturated product.

The program prints:

<!-- snippet: docs/examples/fixed_point.out -->
```text
3 * 0.5                      raw 0x00018000
1 / 3                        raw 0x00005555
0.1 + 0.2                    raw 0x00004ccd
saturated add                raw 0x7fffffff
division by zero             raw 0x7fffffff
```
<!-- /snippet -->

`1 / 3` is `0x5555`, not a third — the representation is finite and the division truncates. This is
the point at which fixed point stops being "floating point with integers": you are choosing exactly
which digits exist, and every machine chooses the same ones.

## What else the promise rests on

Arithmetic alone is not enough. Order of execution has to be derived rather than remembered — that
is the job of [the schedule](tick-and-schedule.md) — and everything the simulation reads has to be
part of the recorded state. Wall-clock time, the address of an allocation, the iteration order of a
hash container keyed by pointer: each of those is a machine-dependent input, and none of them looks
like one at the call site.
