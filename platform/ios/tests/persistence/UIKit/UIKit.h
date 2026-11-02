#pragma once
// Host-only model of the few UIKit selectors used by persistence.mm; Foundation is real.
#import <Foundation/Foundation.h>
typedef NSUInteger UIBackgroundTaskIdentifier;
inline constexpr UIBackgroundTaskIdentifier UIBackgroundTaskInvalid = NSUIntegerMax;
@interface UIApplication : NSObject
@property(class, nonatomic, readonly) UIApplication* sharedApplication;
- (UIBackgroundTaskIdentifier)beginBackgroundTaskWithName:(NSString*)name expirationHandler:(void (^)(void))handler;
- (void)endBackgroundTask:(UIBackgroundTaskIdentifier)identifier;
@end
