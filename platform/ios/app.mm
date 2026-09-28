#import <UIKit/UIKit.h>

@interface GameViewController : UIViewController
@end

// SDK iOS 27 запускает приложение только с жизненным циклом UIScene: окно создаёт сцена, а не
// делегат приложения, и экран берётся из неё, а не из устаревшего UIScreen.mainScreen.
@interface SceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(strong, nonatomic) UIWindow* window;
@end

@implementation SceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)options {
    if (![scene isKindOfClass:UIWindowScene.class]) return;
    self.window = [[UIWindow alloc] initWithWindowScene:static_cast<UIWindowScene*>(scene)];
    self.window.rootViewController = [GameViewController new];
    [self.window makeKeyAndVisible];
}

@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation AppDelegate
@end

int main(int argc, char* argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
