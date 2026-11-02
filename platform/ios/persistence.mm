#import <UIKit/UIKit.h>

#include "persistence.hpp"
#include "haptic.hpp"
#include "mobile_app.hpp"
#include "platform_save.hpp"

void ios_prepare_saves() {
    NSURL* support = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                         inDomains:NSUserDomainMask].firstObject;
    // Framework init creates/verifies the directory. Empty explicitly refuses any HOME/CWD fallback.
    platform::set_save_dir(support.path.UTF8String ?: "");
}

void ios_suspend_and_save(mobile::Game& game) {
    haptic::suspend();
    game.cancel();
    game.suspend();
    UIApplication* app = UIApplication.sharedApplication;
    __block UIBackgroundTaskIdentifier task = UIBackgroundTaskInvalid;
    task = [app beginBackgroundTaskWithName:@"save" expirationHandler:^{
        const UIBackgroundTaskIdentifier expired = task;
        task = UIBackgroundTaskInvalid;
        // end may release the expiration block; do not touch its captures afterwards.
        if (expired != UIBackgroundTaskInvalid) [app endBackgroundTask:expired];
    }];
    // An invalid task is still a chance to try synchronous IO now, never defer to dealloc.
    game.save();
    if (task != UIBackgroundTaskInvalid) [app endBackgroundTask:task];
}
