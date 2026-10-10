#include <array>
#include <cstdio>

#include "codes.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_seat_fixture.hpp"
#include "input_engine.hpp"
#include "lobby.hpp"
#include "verify.hpp"

// Спека #25 В4в: хотсит целиком — устройства через лобби, места в драке, реплей. P1 на клавиатуре с
// первого тика, P2 входит падом посреди уровня, выдернутый пад ставит паузу, P2 уходит. Запись
// воспроизводится на свежей драке тик в тик, а подменённый present расходится ровно на своём тике.
namespace {

using namespace framework::brawl;
namespace replay = framework::replay;
using test::check;
using test::Hotseat;
using ::input::RawEvent;
using ::input::RawKind;

enum Action : int { JUMP = 0, PUNCH = 1 };
enum : uint16_t { KEY_J = 74, KEY_LEFT = 263 };
constexpr uint32_t JOIN = 120, PULL = 160, RESUME = 175, LEAVE = 200, END = 240;

::input::Source key(uint16_t code) { return {::input::SourceKind::Key, code, 1}; }
::input::Source pad(uint16_t code) { return {::input::SourceKind::PadButton, code, 1}; }

::input::ActionLayout p1_layout() {
    ::input::ActionLayout l;
    l.bind(JUMP, key(::input::code::Space));
    l.bind(PUNCH, key(KEY_J));
    l.bind_axis(0, key(::input::code::D), key(::input::code::A), fix32{});
    return l;
}

::input::ActionLayout p2_layout() {
    ::input::ActionLayout l;
    l.bind(PUNCH, key(KEY_LEFT));
    l.bind(JUMP, pad(::input::code::PadA));
    l.bind(PUNCH, pad(::input::code::PadX));
    l.bind_axis(0, {::input::SourceKind::PadAxis, ::input::code::LX, 1}, {}, fix32{});
    return l;
}

struct Session {
    ::input::ActionMap map;
    ::input::InputEngine engine{map};
    framework::input::Lobby lobby{map, SEATS};
    Hotseat sim;
    replay::Stream<BrawlInput> stream;
    std::array<int32_t, END> at{};
    std::array<EntId, END> body{};
    uint64_t seq = 0;

    Session() {
        stream.reset(SEATS);
        check(map.set_layout(0, p1_layout()) && map.set_layout(1, p2_layout()) && lobby.join(0, {-1, true}),
              "P1 sits at the keyboard before the first tick");
    }

    void post(RawKind k, ::input::DeviceKind d, uint8_t slot, uint16_t code, int32_t value = 0) {
        engine.post(RawEvent{k, d, slot, code, value, seq++});
    }
    void pad_event(RawKind k, uint8_t slot, uint16_t code = 0, int32_t value = 0) {
        post(k, ::input::DeviceKind::Gamepad, slot, code, value);
    }
    void keypress(uint16_t code, bool down) {
        post(down ? RawKind::KeyDown : RawKind::KeyUp, ::input::DeviceKind::Keyboard, 0, code);
    }

    BrawlInput read(int p) const {
        BrawlInput in;
        if (!lobby.present(p)) return in;
        const ::input::InputFrame& f = engine.frame(p);
        in.present = true;
        in.move.move_x = f.axes[0];
        in.buttons = f.action_pressed(PUNCH) ? button::ATTACK : 0;
        return in;
    }

    void tick(uint32_t t) {
        engine.drain();
        lobby.update(engine.device());
        for (int p = 0; p < static_cast<int>(SEATS); ++p) engine.resolve(t, p);
        at[t] = -1;
        body[t] = sim.seats.at[1].body;
        if (lobby.paused()) return;
        const std::array<BrawlInput, SEATS> row{read(0), read(1)};
        sim.step(row.data());
        at[t] = static_cast<int32_t>(stream.ticks());
        body[t] = sim.seats.at[1].body;
        check(stream.record(row.data(), sim.hash()), "the stream takes every row");
    }

    void script(uint32_t t) {
        keypress(::input::code::D, t % 40 < 25);
        keypress(::input::code::A, t % 40 >= 25);
        keypress(KEY_J, t % 30 == 5);
        if (t == 80) pad_event(RawKind::DeviceConnected, 0);
        if (t == JOIN) pad_event(RawKind::PadButtonDown, 0, ::input::code::PadX);
        if (t == JOIN + 5) pad_event(RawKind::PadButtonUp, 0, ::input::code::PadX);
        if (t == JOIN + 10) pad_event(RawKind::PadButtonDown, 0, ::input::code::PadX);
        if (t == JOIN + 12) pad_event(RawKind::PadAxis, 0, ::input::code::LX, fix32::from_int(-1).raw);
        if (t == PULL) pad_event(RawKind::DeviceDisconnected, 0);
        if (t == RESUME - 5) pad_event(RawKind::DeviceConnected, 2);
        if (t == RESUME) pad_event(RawKind::PadButtonDown, 2, ::input::code::PadA);
        if (t == LEAVE) lobby.leave(1);
    }

    void play() {
        for (uint32_t t = 0; t < END; ++t) {
            script(t);
            tick(t);
        }
    }

    const BrawlInput& p2(uint32_t t) const { return stream.row(static_cast<replay::Tick>(at[t]))[1]; }
};

void test_the_join_press_is_presence_from_the_next_tick(const Session& s) {
    check(!s.p2(JOIN).present && s.p2(JOIN + 1).present, "P2 is absent on the joining tick and present after it");
    check(s.p2(JOIN + 1).buttons == 0, "the press that seated P2 does not punch on its first tick");
    check(s.p2(JOIN + 10).buttons == button::ATTACK, "a fresh press of the same button punches");
    check(s.p2(JOIN + 13).move.move_x < fix32{}, "P2's stick reaches P2's row");
}

void test_a_pulled_pad_stops_the_fight_and_keeps_the_body(const Session& s) {
    bool held = true;
    for (uint32_t t = PULL; t <= RESUME; ++t) held = held && s.at[t] < 0;
    check(held, "no tick of the fight runs from the pull through the resuming tick");
    check(s.at[RESUME + 1] == s.at[PULL - 1] + 1, "the fight resumes on the tick after the one it stopped on");
    check(!(s.body[PULL - 1] == EntId{}) && s.body[RESUME + 1] == s.body[PULL - 1],
          "P2 resumes in the body that stood through the pause, not a fresh one");
    check(s.lobby.state(1) == framework::input::SeatState::Free && s.body[LEAVE] == EntId{},
          "leaving frees the seat and takes the body off the field");
    check(!s.p2(LEAVE).present && s.stream.ticks() == END - (RESUME + 1 - PULL),
          "leaving reaches the fight on its tick and the pause took no rows");
}

void test_the_recording_replays_on_a_fresh_fight(const Session& s) {
    Hotseat fresh;
    const replay::Verdict v = replay::verify(fresh, s.stream, SEATS);
    check(v.reason == replay::Reason::Match && v.tick == s.stream.ticks(),
          "a fresh fight replays the hotseat recording tick for tick");

    replay::Stream<BrawlInput> forged = s.stream;
    const replay::Tick join = static_cast<replay::Tick>(s.at[JOIN + 1]);
    check(forged.forge_input(join, 1, BrawlInput{}), "the join tick can be forged");
    Hotseat other;
    const replay::Verdict d = replay::verify(other, forged, SEATS);
    check(d.reason == replay::Reason::Diverged && d.tick == join, "a forged absence diverges on the join tick");
}

} // namespace

int main() {
    std::printf("brawl: hotseat join, pause and replay\n");
    Session s;
    s.play();
    test_the_join_press_is_presence_from_the_next_tick(s);
    test_a_pulled_pad_stops_the_fight_and_keeps_the_body(s);
    test_the_recording_replays_on_a_fresh_fight(s);
    return test::verdict("framework-brawl-hotseat");
}
