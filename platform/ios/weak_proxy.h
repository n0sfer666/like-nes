#import <Foundation/Foundation.h>

// CADisplayLink держит цель сильной ссылкой: через прокси контроллер не живёт вечно из-за неё.
@interface WeakProxy : NSObject
+ (instancetype)with:(id)target;
@end
