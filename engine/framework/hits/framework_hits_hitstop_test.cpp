#include "hitstop.hpp"

#include <initializer_list>

#include "framework_hits_check.hpp"
#include "platform_args.hpp"

namespace {

using framework::hits::hitstop_tick;
using framework::hits::test::check;
using Tick = bool (*)(uint16_t&);

bool term(Tick tick) {
    for (uint16_t initial : {uint16_t{0}, uint16_t{1}, uint16_t{3}, uint16_t{65535}}) {
        uint16_t left = initial;
        for (uint32_t i = 0; i <= initial; ++i) {
            if (tick(left) != (i < initial)) return false;
            const uint16_t want = i < initial ? static_cast<uint16_t>(initial - i - 1) : uint16_t{0};
            if (left != want) return false;
        }
        if (tick(left) || left != 0) return false;
    }
    return true;
}

bool loses_last(uint16_t& ticks) {
    if (ticks == 0) return false;
    return --ticks != 0;
}

bool wraps_zero(uint16_t& ticks) { return ticks-- > 0; }

} // namespace

int main(int argc, char** argv) {
    const platform::Args args(argc, argv);
    check(term(hitstop_tick), "zero stays zero; every positive tick freezes, including the last and uint16 max");
    check(!term(loses_last), "control: deciding after the decrement loses the last frozen tick");
    check(!term(wraps_zero), "control: decrementing zero wraps the counter");
    return framework::hits::test::verdict("framework-hits-hitstop");
}
