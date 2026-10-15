#import <UIKit/UIKit.h>

#include <sys/utsname.h>

#include <cstdio>
#include <string>

#include "resim_bench.hpp"

@interface SceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(strong, nonatomic) UIWindow* window;
@end

@implementation SceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)options {
    if (![scene isKindOfClass:UIWindowScene.class]) return;
    self.window = [[UIWindow alloc] initWithWindowScene:static_cast<UIWindowScene*>(scene)];
    self.window.rootViewController = [UIViewController new];
    self.window.rootViewController.view.backgroundColor = UIColor.blackColor;
    [self.window makeKeyAndVisible];

    utsname host = {};
    uname(&host);
    std::printf("[bench] iOS %s, %s\n", UIDevice.currentDevice.systemVersion.UTF8String, host.machine);
    const std::string against = std::string(NSBundle.mainBundle.bundlePath.UTF8String) + "/assets/crossplay.txt";
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        framework::ai::bench::run_resim_bench(against);
        std::fflush(stdout);
    });
}

@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation AppDelegate
@end

int main(int argc, char* argv[]) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
