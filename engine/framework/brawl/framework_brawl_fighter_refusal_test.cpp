#include <cstdio>
#include <string>
#include <vector>

#include "fighter_bake.hpp"
#include "fighter_read.hpp"
#include "framework_brawl_check.hpp"
#include "framework_brawl_fighter_fixture.hpp"

namespace {

using namespace framework::brawl;
using test::check;

struct Refusal {
    int line;
    const char* replacement;
    int expect_line;
    const char* message;
};

const Refusal REFUSALS[] = {
    {4, "speed_x | fast", 4, "speed_x must be a decimal number"},
    {4, "speed_x | 17", 4, "speed_x is outside the range the engine accepts"},
    {4, "speed_x | 2 | 3", 4, "expected '<key> | <value>'"},
    {4, "speed_x |", 4, "an empty field"},
    {7, "gravity | 0", 7, "gravity is outside the range the engine accepts"},
    {9, "depth | 65", 9, "depth is outside the range the engine accepts"},
    {10, "hp | -1", 10, "hp must be a whole number"},
    {10, "hp | 10000", 10, "hp is outside the range the engine accepts"},
    {10, "hp | 0", 10, "hp is outside the range the engine accepts"},
    {11, "down | soon", 11, "down must be a whole number"},
    {11, "down | 0", 11, "down is outside the range the engine accepts"},
    {12, "getup | 121", 12, "getup is outside the range the engine accepts"},
    {11, "", 12, "the fighter is missing down"},
    {12, "", 11, "the fighter is missing getup"},
    {3, "sheet | rainbird", 3, "no clip of sheet 'rainbird' in the clips of this manifest"},
    {3, "sheet | rain/bird", 3, "sheet must be a name without '/' or control characters"},
    {3, "sheet | rain\x01" "bird", 3, "sheet must be a name without '/' or control characters"},
    {10, "power | 3", 10, "unknown key 'power'"},
    {10, "speed_x | 2", 10, "speed_x is set twice"},
    {3, "", 12, "the fighter is missing sheet"},
    {14, "", 15, "unknown key 'type'"},
    {15, "type | sweep", 15, "type must be light, heavy, launch, grab or throw"},
    {15, "sheet | banderas", 15, "unknown key 'sheet' in a move"},
    {16, "damage | 1000", 16, "damage is outside the range the engine accepts"},
    {17, "depth | 0", 17, "depth is outside the range the engine accepts"},
    {19, "hitstun | 121", 19, "hitstun is outside the range the engine accepts"},
    {21, "knock_y | -17", 21, "knock_y is outside the range the engine accepts"},
    {22, "hits_down | maybe", 22, "hits_down must be yes or no"},
    {22, "", 21, "move 'jab' hit0 is missing hits_down"},
    {32, "", 31, "move 'kick' hit1 is missing hits_down"},
    {24, "move | jab | hit0", 24, "move 'jab' hit0 is declared twice"},
    {24, "move | kick | hit4", 24, "move box 'hit4' must be hit0..hit3"},
    {24, "move | kick", 24, "expected 'move | <clip> | hit<N>'"},
    {24, "move | ki/ck | hit1", 24, "a move clip must be a name without '/' or control characters"},
    {24, "move | ki\x7f" "ck | hit1", 24, "a move clip must be a name without '/' or control characters"},
};

std::string edge_text(const std::string& speed, const std::string& gravity, const std::string& depth,
                      const std::string& hp, const std::string& ticks, const std::string& move) {
    return "sheet | banderas\nspeed_x | " + speed + "\nspeed_z | " + speed + "\nrun_x | " + speed +
           "\ngravity | " + gravity + "\njump_vy | " + speed + "\ndepth | " + depth + "\nhp | " + hp +
           "\ndown | " + ticks + "\ngetup | " + ticks + "\nmove | jab | hit0\n" + move;
}

void edge(const char* what, const std::string& text, fix32 speed, fix32 gravity, fix32 depth, uint32_t hp,
          uint32_t ticks, const Strike& want) {
    std::vector<uint8_t> out;
    FighterBakeError err;
    const bool baked = bake_fighter("edge.fighter", text, test::fixture_clips(), out, err);
    if (!baked) std::printf("  got line %d: %s\n", err.line, err.message.c_str());
    FighterTable t;
    Strike s;
    const DepthProfile p = baked && t.open(out.data(), out.size()) ? t.profile() : DepthProfile{};
    const bool head = p.speed_x == speed && p.speed_z == speed && t.run_x() == speed && p.jump_vy == speed &&
                      p.gravity == gravity && t.depth() == depth && t.hp() == hp &&
                      t.down_ticks() == ticks && t.getup_ticks() == ticks;
    check(baked && head && t.move(0, s) && test::same_strike(s, want), what);
}

void edges() {
    edge("every field at its upper limit bakes and reads back",
         edge_text("16", "16", "64", "9999", "120",
                   "type | throw\ndamage | 999\ndepth | 64\nhitstop | 120\nhitstun | 120\nknock_x | 16\n"
                   "knock_y | 16\nhits_down | yes\n"),
         MAX_FIGHTER_SPEED, MAX_FIGHTER_SPEED, MAX_FIGHTER_DEPTH, MAX_HP, MAX_HIT_TICKS,
         Strike{0, HitType::Throw, true, MAX_DAMAGE, MAX_FIGHTER_DEPTH, MAX_HIT_TICKS, MAX_HIT_TICKS,
                MAX_FIGHTER_SPEED, MAX_FIGHTER_SPEED});
    edge("every field at its lower limit bakes and reads back",
         edge_text("0", "1", "1", "1", "1",
                   "type | light\ndamage | 0\ndepth | 1\nhitstop | 0\nhitstun | 0\nknock_x | -16\n"
                   "knock_y | -16\nhits_down | no\n"),
         fix32::from_int(0), fix32::from_int(1), fix32::from_int(1), MIN_HP, 1,
         Strike{0, HitType::Light, false, 0, fix32::from_int(1), 0, 0, -MAX_FIGHTER_SPEED, -MAX_FIGHTER_SPEED});
}

std::string with_line(int number, const std::string& replacement) {
    const std::string text = test::FIGHTER_TEXT;
    std::string out;
    int line = 1;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::size_t end = nl == std::string::npos ? text.size() : nl;
        out += line == number ? replacement : text.substr(pos, end - pos);
        if (nl == std::string::npos) break;
        out += '\n';
        pos = nl + 1;
        ++line;
    }
    return out;
}

void expect(const std::string& text, const std::vector<framework::graphics::ClipSrc>& clips, int line,
            const std::string& message) {
    std::vector<uint8_t> out;
    FighterBakeError err;
    const bool baked = bake_fighter("banderas.fighter", text, clips, out, err);
    const bool ok = !baked && err.line == line && err.message == message;
    if (!ok) std::printf("  got line %d: %s\n", err.line, err.message.c_str());
    check(ok, message.c_str());
}

} // namespace

int main() {
    const std::vector<framework::graphics::ClipSrc> clips = test::fixture_clips();
    for (const Refusal& r : REFUSALS) expect(with_line(r.line, r.replacement), clips, r.expect_line, r.message);
    expect("", clips, 0, "the fighter file is empty");
    expect("# nothing here\n\n", clips, 2, "the fighter file is empty");

    std::vector<framework::graphics::ClipSrc> no_kick = clips;
    no_kick.pop_back();
    expect(test::FIGHTER_TEXT, no_kick, 24, "move 'kick' hit1: no clip 'banderas/kick' in the clips of this manifest");
    std::vector<framework::graphics::ClipSrc> wrong_box = clips;
    wrong_box.back() = test::fixture_clip("banderas/kick", 0);
    expect(test::FIGHTER_TEXT, wrong_box, 24,
           "move 'kick' hit1: no frame of clip 'banderas/kick' carries this hit box");

    const std::string head = std::string(test::FIGHTER_TEXT).substr(0, std::string(test::FIGHTER_TEXT).find("move"));
    std::vector<uint8_t> out;
    FighterBakeError err;
    check(bake_fighter("bare.fighter", head, clips, out, err), "a fighter without moves bakes");
    edges();
    check(with_line(1, "") == test::FIGHTER_TEXT, "the line splicer keeps the text it does not touch");
    return test::verdict("framework-brawl-fighter-refusal");
}
