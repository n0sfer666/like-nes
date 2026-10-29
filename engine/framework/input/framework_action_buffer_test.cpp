#include "action_buffer.hpp"

#include <cstdio>

#include "platform_args.hpp"

// Спека #29 В2б: буфер нажатия — ядро ввода SDK; цепочки ударов, которые на нём строит драка, — игра.
// Срок истекает в пустой буфер, и это утверждение обязано отбить сломанную копию без сброса на нуле.
namespace {

using framework::input::ActionBuffer;
using framework::input::buffer_action;
using framework::input::clear_action;
using framework::input::NO_ACTION;
using framework::input::take_action;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

bool empty(const ActionBuffer& b) { return b.action == NO_ACTION && b.ticks == 0; }

void test_an_empty_buffer_never_calls_use() {
    ActionBuffer b;
    int calls = 0;
    check(empty(b), "a new buffer holds no action");
    check(!take_action(b, [&calls](uint16_t) { return ++calls > 0; }) && calls == 0,
          "taking from an empty buffer is false and does not call use");
}

void test_a_used_action_clears_the_buffer() {
    ActionBuffer b;
    buffer_action(b, 5, 3);
    uint16_t got = NO_ACTION;
    check(take_action(b, [&got](uint16_t a) {
        got = a;
        return true;
    }),
          "a used action is taken");
    check(got == 5 && empty(b), "use sees the buffered action and the buffer empties at once");
}

void test_a_newer_action_replaces_the_older() {
    ActionBuffer b;
    buffer_action(b, 5, 3);
    buffer_action(b, 7, 2);
    check(b.action == 7 && b.ticks == 2, "buffering again replaces the action and its term");
    clear_action(b);
    check(empty(b), "clear empties the buffer");
}

// Сломанная копия: нулевой срок кладёт действие как есть, и буфер не пуст, хотя взять его нельзя.
void buffer_action_raw(ActionBuffer& buf, uint16_t action, uint16_t ticks) { buf = ActionBuffer{action, ticks}; }

template <typename Put>
bool zero_term_is_empty(Put put) {
    ActionBuffer b;
    put(b, 7, 3);
    put(b, 5, 0);
    return empty(b);
}

void test_a_zero_term_buffers_nothing() {
    check(zero_term_is_empty(buffer_action), "an action with a zero term empties the buffer");
    check(!zero_term_is_empty(buffer_action_raw), "control: a buffer that keeps a zero-term action is not empty");
}

// Сломанная копия: срок уменьшается, но на нуле действие остаётся висеть.
template <typename Use>
bool take_action_stale(ActionBuffer& buf, Use use) {
    if (buf.ticks == 0) return false;
    if (use(buf.action)) {
        clear_action(buf);
        return true;
    }
    --buf.ticks;
    return false;
}

template <typename Take>
bool expires_empty(Take take) {
    ActionBuffer b;
    buffer_action(b, 5, 3);
    int calls = 0;
    bool taken = false;
    for (int k = 0; k < 4; ++k) taken = take(b, [&calls](uint16_t) { return ++calls < 0; }) || taken;
    return !taken && calls == 3 && empty(b);
}

void test_the_term_runs_out_into_an_empty_buffer() {
    check(expires_empty([](ActionBuffer& b, auto use) { return take_action(b, use); }),
          "an unused action loses a tick per take and expires into an empty buffer after its term");
    check(!expires_empty([](ActionBuffer& b, auto use) { return take_action_stale(b, use); }),
          "control: a buffer without the reset at zero keeps a stale action");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework action buffer: an action waits its term for a tick it can start on\n");
    test_an_empty_buffer_never_calls_use();
    test_a_used_action_clears_the_buffer();
    test_a_newer_action_replaces_the_older();
    test_a_zero_term_buffers_nothing();
    test_the_term_runs_out_into_an_empty_buffer();
    std::printf("framework-action-buffer: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
