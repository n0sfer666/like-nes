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
    game_.set_demo([NSProcessInfo.processInfo.arguments containsObject:@"--demo"]);
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
        game_.pointer(reinterpret_cast<intptr_t>(t), phase, static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(sz.width), static_cast<float>(sz.height));
    }
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
