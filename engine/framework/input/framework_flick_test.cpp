#include "flick.hpp"

#include <cstdio>
#include <vector>

#include "platform_args.hpp"

// Спека #29 В2б: рывок оси из покоя — ядро ввода SDK, а не жанр драки. Случаи правила идут таблицей,
// и таблица обязана отбить сломанную копию без требования покоя: иначе зелёный прогон не отличим от
// проверки, которая принимает любой разгон за рывок.
namespace {

using framework::input::FlickRule;
using ::input::InputFrame;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

enum Action : int { BLOCK = 3 };
enum Axis : int { MOVE_X = 1 };
constexpr uint32_t K = 3;
const fix32 ONE = fix32::from_int(1);
const fix32 T = fix32::from_int(3) / fix32::from_int(4);
const fix32 UNDER = fix32::from_raw(T.raw - 1);
const FlickRule RULE{BLOCK, MOVE_X, T, K};

fix32 tenths(int n) { return fix32::from_int(n) / fix32::from_int(10); }

InputFrame frame(fix32 x, bool block = true) {
    InputFrame f;
    f.axes[MOVE_X] = x;
    if (block) f.held = uint64_t{1} << BLOCK;
    return f;
}

struct Case {
    const char* what;
    std::vector<InputFrame> frames;
    FlickRule rule;
    bool want;
};

const std::vector<Case>& cases() {
    static const std::vector<Case> all{
        {"rest to the threshold in exactly K ticks flicks", {frame({}), frame(tenths(3)), frame(tenths(6)), frame(T)}, RULE, true},
        {"one tick slower than K does not flick, though the rest frame is in the span",
         {frame({}), frame(tenths(2)), frame(tenths(4)), frame(tenths(6)), frame(T)}, RULE, false},
        {"a keyboard axis, rest to full in one tick, flicks", {frame({}), frame(ONE)}, RULE, true},
        {"a flick to the left flicks", {frame({}), frame(-T)}, RULE, true},
        {"one raw step under the threshold does not flick", {frame({}), frame(UNDER)}, RULE, false},
        {"one raw step under the threshold to the left does not flick", {frame({}), frame(-UNDER)}, RULE, false},
        {"a flick without block does not flick", {frame({}), frame(ONE, false)}, RULE, false},
        {"block pressed after the axis crossed does not flick", {frame({}, false), frame(ONE, false), frame(ONE)}, RULE, false},
        {"block pressed on the flick tick flicks", {frame({}, false), frame(ONE)}, RULE, true},
        {"holding the axis past the threshold does not flick again", {frame({}), frame(ONE), frame(ONE)}, RULE, false},
        {"a swing across without rest is not a flick", {frame(-ONE), frame(ONE)}, RULE, false},
        {"a full swing left and back right within K ticks is not a flick from rest",
         {frame({}), frame(-ONE), frame(tenths(-5)), frame(tenths(8))}, RULE, false},
        {"a span without a previous tick does not flick", {frame(ONE)}, RULE, false},
        {"an empty span does not flick", {}, RULE, false},
        {"an axis past the frame never flicks", {frame({}), frame(ONE)}, FlickRule{BLOCK, ::input::MAX_AXES, T, K}, false},
        {"a negative axis never flicks", {frame({}), frame(ONE)}, FlickRule{BLOCK, -1, T, K}, false},
        {"a block past the bitset never flicks", {frame({}), frame(ONE)}, FlickRule{::input::MAX_ACTIONS, MOVE_X, T, K}, false},
        {"a negative block never flicks", {frame({}), frame(ONE)}, FlickRule{-1, MOVE_X, T, K}, false},
    };
    return all;
}

bool reaches(const InputFrame& f, const FlickRule& r) {
    const fix32 v = f.axes[r.axis];
    return !((v < fix32{} ? -v : v) < r.threshold);
}

// Сломанная копия: покой оси до рывка не требуется, хватает того, что в окне её не было за порогом.
bool flick_without_rest(std::span<const InputFrame> recent, const FlickRule& rule) {
    if (rule.axis < 0 || rule.axis >= ::input::MAX_AXES || rule.hold < 0 || rule.hold >= ::input::MAX_ACTIONS)
        return false;
    const size_t n = recent.size();
    if (n < 2 || !recent[n - 1].action_held(rule.hold) || !reaches(recent[n - 1], rule)) return false;
    const size_t from = n - 1 > rule.ticks ? n - 1 - rule.ticks : 0;
    for (size_t i = n - 1; i-- > from;)
        if (reaches(recent[i], rule)) return false;
    return true;
}

using Flick = bool (*)(std::span<const InputFrame>, const FlickRule&);

int misses(Flick f, bool verbose) {
    int missed = 0;
    for (const Case& c : cases()) {
        if (f(c.frames, c.rule) == c.want) continue;
        ++missed;
        if (verbose) std::printf("  FAIL: %s\n", c.what);
    }
    return missed;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework flick: an axis from rest past the threshold within K ticks\n");
    check(misses(framework::input::flick, true) == 0, "every flick case holds");
    check(misses(flick_without_rest, false) > 0, "control: a flick without rest misses the table");
    if (fails == 0) std::printf("framework-flick: PASS - %zu cases\n", cases().size());
    else std::printf("framework-flick: FAIL\n");
    return fails == 0 ? 0 : 1;
}
