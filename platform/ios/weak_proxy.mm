#import "weak_proxy.h"

@implementation WeakProxy {
    __weak id target_;
}
+ (instancetype)with:(id)target {
    WeakProxy* p = [self new];
    p->target_ = target;
    return p;
}
- (id)forwardingTargetForSelector:(SEL)sel { return target_; }
@end
