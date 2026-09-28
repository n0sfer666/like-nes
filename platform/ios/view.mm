#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <webgpu/webgpu.h>

#include "gpu.hpp"
#include "mobile_game.hpp"

using namespace game;

@interface MetalView : UIView
@end
@implementation MetalView
+ (Class)layerClass { return [CAMetalLayer class]; }
@end

@interface WeakProxy : NSObject
+ (instancetype)with:(id)target;
@end
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

@interface GameViewController : UIViewController {
    GpuContext gpu_;
    MobileGame game_;
    WGPUSurface surface_;
    CADisplayLink* link_;
    bool started_;
    bool probe_;
    NSString* run_;
}
@end

@implementation GameViewController

- (void)loadView {
    self.view = [[MetalView alloc] initWithFrame:CGRectZero];
    self.view.multipleTouchEnabled = YES;   // стик + кнопка-огонь одновременно
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if (started_) return;

    CAMetalLayer* layer = static_cast<CAMetalLayer*>(self.view.layer);
    const CGFloat scale = self.traitCollection.displayScale;
    layer.contentsScale = scale;
    const uint32_t w = static_cast<uint32_t>(self.view.bounds.size.width * scale);
    const uint32_t h = static_cast<uint32_t>(self.view.bounds.size.height * scale);
    layer.drawableSize = CGSizeMake(w, h);

    gpu_.instance = wgpuCreateInstance(nullptr);
    WGPUSurfaceDescriptorFromMetalLayer ml = {};
    ml.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
    ml.layer = (__bridge void*)layer;
    WGPUSurfaceDescriptor sd = {};
    sd.nextInChain = &ml.chain;
    surface_ = wgpuInstanceCreateSurface(gpu_.instance, &sd);
    if (!gpu_.init(surface_)) { NSLog(@"[game] gpu init failed"); [self releaseGpu]; return; }
    if (!game_.init(gpu_, surface_, w, h)) { NSLog(@"[game] game init failed"); [self releaseGpu]; return; }
    NSArray<NSString*>* args = NSProcessInfo.processInfo.arguments;
    game_.set_demo([args containsObject:@"--demo"]);
    probe_ = [args containsObject:@"--touch-probe"];
    run_ = NSProcessInfo.processInfo.environment[@"LIKE_NES_TOUCH_RUN"] ?: @"-";
    started_ = true;
    NSLog(@"[game] iOS shell up: %ux%u - left = stick, bottom-right = fire", w, h);

    link_ = [CADisplayLink displayLinkWithTarget:[WeakProxy with:self] selector:@selector(frame)];
    [link_ addToRunLoop:NSRunLoop.mainRunLoop forMode:NSDefaultRunLoopMode];

    NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
    [nc addObserver:self selector:@selector(pause) name:UISceneDidEnterBackgroundNotification object:nil];
    [nc addObserver:self selector:@selector(resume) name:UISceneWillEnterForegroundNotification object:nil];
}

// Отказ init освобождает поднятое: следующий viewDidAppear создаст инстанс заново, а не поверх.
- (void)releaseGpu {
    if (surface_) wgpuSurfaceRelease(surface_);
    surface_ = nullptr;
    gpu_.shutdown();
}

// Фоновый процесс система убивает без dealloc: сохраняться надо здесь, а не при выходе.
- (void)pause {
    link_.paused = YES;
    game_.suspend();
}
- (void)resume { link_.paused = NO; }

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [link_ invalidate];
    game_.shutdown();
    [self releaseGpu];
}

- (void)frame {
    if (started_) game_.frame(surface_);
}

- (void)dispatch:(NSSet<UITouch*>*)touches phase:(MobileGame::Touch)phase {
    if (!started_) return;
    const CGSize sz = self.view.bounds.size;
    for (UITouch* t in touches) {
        const CGPoint p = [t locationInView:self.view];
        // Граница ObjC: id касания — адрес UITouch, UIKit держит один объект на всё касание.
        const intptr_t id = reinterpret_cast<intptr_t>(t);
        game_.pointer(id, phase, static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(sz.width), static_cast<float>(sz.height));
        if (probe_) [self probe:id phase:phase at:p];
    }
}

// Строка на касание с состоянием ПОСЛЕ него: XCUITest проигрывает жест целиком и середину не видит,
// поэтому гейт (`scripts/ios_sim_gate.sh`) судит историю из системного лога. id32 — усечение до
// int, каким id был до B8: печатается как улика, что оно не склеивает соседние касания. run —
// нонс прогона гейта: строки прошлых запусков в той же выборке лога вердикт отбрасывает.
- (void)probe:(intptr_t)id phase:(MobileGame::Touch)phase at:(CGPoint)p {
    static const char* const kPhase[] = {"down", "move", "up"};
    NSLog(@"[touch] %s id=%ld id32=%d x=%.0f y=%.0f stick=%ld fire=%ld run=%@", kPhase[static_cast<int>(phase)],
          static_cast<long>(id), static_cast<int32_t>(id), p.x, p.y,
          static_cast<long>(game_.stick_touch()), static_cast<long>(game_.fire_touch()), run_);
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self dispatch:touches phase:MobileGame::Touch::Down];
}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self dispatch:touches phase:MobileGame::Touch::Move];
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self dispatch:touches phase:MobileGame::Touch::Up];
}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self dispatch:touches phase:MobileGame::Touch::Up];
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskPortrait;
}

@end
