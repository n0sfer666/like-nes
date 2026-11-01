#include "struck_list.hpp"

#include "framework_hits_check.hpp"
#include "platform_args.hpp"

namespace {

using namespace framework::hits;
using framework::core::EntId;
using framework::hits::test::check;

template <uint32_t N, typename Add>
bool capacity(Add add) {
    StruckList<N> list;
    for (uint32_t i = 0; i < N; ++i)
        if (!add(list, EntId{100 + i}, static_cast<uint8_t>(i)) || list.count != i + 1) return false;
    if (add(list, EntId{999}, 9) || list.count != N) return false;
    for (uint32_t i = 0; i < N; ++i)
        if (list.at[i].seq != 100 + i || list.at[i].box != i) return false;
    return true;
}

bool stores_pair(bool (*has)(std::span<const Struck>, EntId, uint8_t)) {
    StruckList<3> list;
    if (has(list.at, EntId{20}, 1) || list.has(EntId{20}, 1)) return false;
    list.add(EntId{20}, 1);
    list.add(EntId{20}, 2);
    list.add(EntId{30}, 1);
    return has(list.at, EntId{20}, 1) && has(list.at, EntId{20}, 2) && has(list.at, EntId{30}, 1) &&
           !has(list.at, EntId{20}, 3) && !has(list.at, EntId{40}, 1) &&
           list.has(EntId{20}, 2) && !list.has(EntId{20}, 3);
}

bool target_only(std::span<const Struck> at, EntId target, uint8_t) {
    for (const Struck& s : at)
        if (s.seq == target.seq) return true;
    return false;
}

bool compaction(void (*forget)(std::span<Struck>, uint32_t&, uint8_t)) {
    StruckList<5> list;
    list.add(EntId{10}, 1);
    list.add(EntId{20}, 2);
    list.add(EntId{30}, 1);
    list.add(EntId{40}, 3);
    list.add(EntId{50}, 1);
    forget(list.at, list.count, 9);
    if (list.count != 5 || list.at[4].seq != 50) return false;
    forget(list.at, list.count, 1);
    if (list.count != 2 || list.at[0].seq != 20 || list.at[0].box != 2 ||
        list.at[1].seq != 40 || list.at[1].box != 3) return false;
    for (uint32_t i = list.count; i < list.at.size(); ++i)
        if (list.at[i].seq != 0 || list.at[i].box != 0) return false;
    if (!list.add(EntId{60}, 1) || !list.has(EntId{60}, 1) || list.has(EntId{10}, 1)) return false;
    list.forget(2);
    list.forget(3);
    list.forget(1);
    list.forget(1);
    if (list.count != 0) return false;
    for (const Struck& s : list.at)
        if (s.seq != 0 || s.box != 0) return false;
    return true;
}

// Сломанная копия сохраняет порядок, но оставляет старые строки в хвосте: его хеширует Body.
void dirty_tail(std::span<Struck> at, uint32_t& count, uint8_t box) {
    uint32_t kept = 0;
    for (uint32_t i = 0; i < count; ++i)
        if (at[i].box != box) at[kept++] = at[i];
    count = kept;
}

void clears_all(std::span<Struck> at, uint32_t& count, uint8_t) {
    for (Struck& s : at) s = Struck{};
    count = 0;
}

void test_live_prefix() {
    StruckList<3> list;
    list.at[2] = Struck{42, 7};
    check(!list.has(EntId{42}, 7), "has reads only the live prefix");
    check(has_struck(list.at, EntId{42}, 7), "control: reading the whole storage sees a stale tail");
    StruckList<0> empty;
    empty.forget(1);
    check(empty.count == 0 && !empty.has(EntId{1}, 1), "zero-capacity storage has an empty live prefix");
}

} // namespace

int main(int argc, char** argv) {
    const platform::Args args(argc, argv);
    const auto add = [](auto& list, EntId id, uint8_t box) { return list.add(id, box); };
    check(capacity<0>(add) && capacity<3>(add) && capacity<16>(add) && capacity<17>(add),
          "capacity is a parameter: zero, small, default and above the old limit refuse without overwrite");
    check(!capacity<17>([](auto& list, EntId id, uint8_t box) { return list.count < 16 && list.add(id, box); }),
          "control: the old hardcoded limit loses the seventeenth slot");
    check(StruckList<>{}.at.size() == 16, "default storage keeps sixteen rows");
    check(stores_pair(has_struck), "a target and box form a pair, different boxes on one target are independent");
    check(!stores_pair(target_only), "control: ignoring the box merges independent activations");
    check(compaction(forget_struck), "forget is stable, clears the vacated tail and allows reuse");
    check(!compaction(dirty_tail) && !compaction(clears_all), "control: stale tail and indiscriminate clearing are detected");
    test_live_prefix();
    return framework::hits::test::verdict("framework-hits-struck");
}
