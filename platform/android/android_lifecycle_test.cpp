#include "android_lifecycle.hpp"
#include "mobile_app.hpp"

#include <android_native_app_glue.h>

#include <cstdio>
#include <string>

namespace {
struct Game final : mobile::Game {
    std::string calls;
    bool init(GpuContext&) override { return true; }
    bool attach_surface(WGPUSurface, uint32_t, uint32_t, const mobile::SafeArea&) override { return true; }
    void detach_surface() override {}
    void resize(uint32_t, uint32_t, const mobile::SafeArea&) override {}
    void pointer(intptr_t, mobile::Touch, float, float) override {}
    void cancel() override { calls += "cancel;"; }
    void frame(double) override {}
    bool wants_frames() const override { return false; }
    void suspend() override { calls += "pause;"; }
    void resume() override { calls += "resume;"; }
    void save() override { calls += "write;"; }
    void shutdown() override {}
};
}
int main() {
    Game game;
    int fails = 0;
    auto event = [&](int32_t cmd, const char* expected) {
        game.calls.clear(); android_game_lifecycle(&game, cmd);
        if (game.calls != expected) ++fails;
    };
    event(APP_CMD_LOST_FOCUS, "cancel;");
    event(APP_CMD_PAUSE, "cancel;pause;");
    event(APP_CMD_SAVE_STATE, "write;");
    event(APP_CMD_STOP, "write;");
    event(APP_CMD_STOP, "write;"); // STOP repeats a completed SAVE_STATE/earlier STOP safely.
    event(APP_CMD_RESUME, "resume;");
    event(APP_CMD_INIT_WINDOW, "");
    event(APP_CMD_TERM_WINDOW, "");
    event(APP_CMD_DESTROY, "");
    android_game_lifecycle(nullptr, APP_CMD_SAVE_STATE);
    std::puts(fails ? "android-lifecycle: FAIL" : "android-lifecycle: PASS pause has no IO, synchronous SAVE_STATE and STOP, repeat and no-game");
    return fails ? 1 : 0;
}
