#import <UIKit/UIKit.h>

#include "mobile_app.hpp"
#include "persistence.hpp"
#include "platform_save.hpp"

#include <cstdio>

namespace {
std::string calls, directory;
bool grant, active, expire_on_write;
int fails;
void (^expiration)(void);
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("ios-persistence: FAIL %s\n", why); } }
struct Game final : mobile::Game {
    bool init(GpuContext&) override { return true; }
    bool attach_surface(WGPUSurface, uint32_t, uint32_t, const mobile::SafeArea&) override { return true; }
    void detach_surface() override {}
    void resize(uint32_t, uint32_t, const mobile::SafeArea&) override {}
    void pointer(intptr_t, mobile::Touch, float, float) override {}
    void frame(double) override {}
    bool wants_frames() const override { return false; }
    void cancel() override { calls += "cancel;"; }
    void suspend() override { calls += "pause;"; }
    void save() override {
        check(!grant || active, "granted background task already active when synchronous write starts");
        calls += "write;";
        if (expire_on_write) expiration();
    }
    void resume() override {}
    void shutdown() override {}
};
}
namespace platform { bool set_save_dir(std::string path) { directory = std::move(path); return !directory.empty(); } }
namespace haptic { void suspend() { calls += "haptic;"; } }

@implementation UIApplication
+ (UIApplication*)sharedApplication {
    static UIApplication* app = [[UIApplication alloc] init];
    return app;
}
- (UIBackgroundTaskIdentifier)beginBackgroundTaskWithName:(NSString*)name expirationHandler:(void (^)(void))handler {
    check([name isEqualToString:@"save"] && !active, "one named background task at a time");
    calls += "begin;"; expiration = [handler copy]; active = grant;
    return grant ? 7 : UIBackgroundTaskInvalid;
}
- (void)endBackgroundTask:(UIBackgroundTaskIdentifier)identifier {
    check(active && identifier == 7, "end only granted task, once");
    calls += "end;"; active = false; expiration = nil;
}
@end

int main() {
    @autoreleasepool {
        ios_prepare_saves();
        check(directory.starts_with('/') && directory.ends_with("/Library/Application Support"), "shell chooses Application Support, no Documents/CWD");
        Game game;
        for (int mode = 0; mode < 3; ++mode) {
            grant = mode != 1; expire_on_write = mode == 2; calls.clear();
            ios_suspend_and_save(game);
            check(calls == (grant ? "haptic;cancel;pause;begin;write;end;" : "haptic;cancel;pause;begin;write;"),
                "pause before save, synchronous write before end, best effort after refused grant");
            check(!active, "task released after completed/expired write");
            expiration = nil;
        }
        grant = true; expire_on_write = false; calls.clear();
        ios_suspend_and_save(game); ios_suspend_and_save(game);
        check(calls == "haptic;cancel;pause;begin;write;end;haptic;cancel;pause;begin;write;end;", "repeat lifecycle balances each task");
    }
    std::puts(fails ? "ios-persistence: FAIL" : "ios-persistence: PASS actual shell code, modeled grant/refusal/expiry/repeat, no live UIKit claim");
    return fails ? 1 : 0;
}
