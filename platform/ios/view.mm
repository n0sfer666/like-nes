#import <UIKit/UIKit.h>

#include <webgpu/webgpu.h>

#include "gpu.hpp"
#include "mobile_app.hpp"

#import "metal_view.h"
#import "touch_probe.h"
#import "weak_proxy.h"
#import "view_layout.hpp"

// Оболочка не знает, какую игру держит: всё игровое — за mobile::Game из make_game().
@interface GameViewController : UIViewController {
    GpuContext gpu_;
    std::unique_ptr<mobile::Game> game_;
    WGPUSurface surface_;
    CADisplayLink* link_;
    CFTimeInterval last_;
    IosViewLayout layout_;
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

- (bool)makeSurface {
    (void)layout_.update(self.view);
    WGPUSurfaceDescriptorFromMetalLayer ml = {};
    ml.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
    ml.layer = (__bridge void*)self.view.layer;
    WGPUSurfaceDescriptor sd = {};
    sd.nextInChain = &ml.chain;
    surface_ = wgpuInstanceCreateSurface(gpu_.instance, &sd);
    return surface_ != nullptr;
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if (started_) return;

    gpu_.instance = wgpuCreateInstance(nullptr);
    if (![self makeSurface] || !gpu_.init(surface_)) { NSLog(@"[game] gpu init failed"); [self releaseGpu]; return; }
    game_ = mobile::make_game();
    if (!game_->init(gpu_) || !game_->attach_surface(surface_, layout_.width, layout_.height, layout_.safe)) {
        NSLog(@"[game] game init failed");
        [self teardown];
        return;
    }
    probe_ = [NSProcessInfo.processInfo.arguments containsObject:@"--touch-probe"];
    run_ = NSProcessInfo.processInfo.environment[@"LIKE_NES_TOUCH_RUN"] ?: @"-";
    if (probe_) probe_expose_safe_area(self.view);
    started_ = true;
    NSLog(@"[game] iOS shell up: %ux%u", layout_.width, layout_.height);

    link_ = [CADisplayLink displayLinkWithTarget:[WeakProxy with:self] selector:@selector(frame)];
    [link_ addToRunLoop:NSRunLoop.mainRunLoop forMode:NSDefaultRunLoopMode];

    NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
    [nc addObserver:self selector:@selector(pause) name:UISceneDidEnterBackgroundNotification object:nil];
    [nc addObserver:self selector:@selector(resume) name:UISceneWillEnterForegroundNotification object:nil];
    [nc addObserver:self selector:@selector(teardown) name:UIApplicationWillTerminateNotification object:nil];
}

- (void)pace {
    link_.paused = !game_->wants_frames();
}

- (void)releaseSurface {
    if (surface_) wgpuSurfaceRelease(surface_);
    surface_ = nullptr;
}

// Отказ init освобождает поднятое: следующий viewDidAppear создаст инстанс заново, а не поверх.
- (void)releaseGpu {
    [self releaseSurface];
    gpu_.shutdown();
}

// Фоновый процесс система убивает без dealloc: сохраняться надо здесь, а не при выходе. Поверхность
// снимается тем же путём, что и окно Android на APP_CMD_TERM_WINDOW: игра переживает её потерю.
- (void)pause {
    if (!started_ || !surface_) return;
    link_.paused = YES;
    game_->cancel();
    game_->suspend();
    game_->detach_surface();
    [self releaseSurface];
}

- (void)resume {
    if (!started_ || surface_) return;
    if (![self makeSurface] || !game_->attach_surface(surface_, layout_.width, layout_.height, layout_.safe)) {
        NSLog(@"[game] surface attach failed");
        [self releaseSurface];
        return;
    }
    game_->resume();
    last_ = 0;
    [self pace];
}

- (void)updateLayout:(BOOL)force {
    if (!started_ || !surface_) return;
    const bool changed = layout_.update(self.view);
    if (!force && !changed) return;
    // При 180° размер кадра прежний, но вырез и координаты удержанного пальца уже другие.
    game_->cancel();
    game_->resize(layout_.width, layout_.height, layout_.safe);
    if (probe_) probe_expose_safe_area(self.view);
    [self pace];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    [self updateLayout:NO];
}

- (void)viewSafeAreaInsetsDidChange {
    [super viewSafeAreaInsetsDidChange];
    [self updateLayout:NO];
}

- (void)viewWillTransitionToSize:(CGSize)size
      withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    if (started_) game_->cancel();
    [coordinator animateAlongsideTransition:nil completion:^(id<UIViewControllerTransitionCoordinatorContext>) {
        [self updateLayout:YES];
    }];
}

// Идемпотентно: зовётся и уведомлением о завершении, и из dealloc.
- (void)teardown {
    [link_ invalidate];
    link_ = nil;
    started_ = false;
    if (game_) {
        game_->detach_surface();
        game_->shutdown();
        game_.reset();
    }
    [self releaseGpu];
}

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [self teardown];
}

- (void)frame {
    if (!started_) return;
    const CFTimeInterval now = link_.timestamp;
    const double dt = last_ > 0 ? now - last_ : 1.0 / 60;
    last_ = now;
    game_->frame(dt);
    [self pace];
}

- (void)dispatch:(NSSet<UITouch*>*)touches phase:(mobile::Touch)phase {
    if (!started_) return;
    for (UITouch* t in touches) {
        const CGPoint p = [t locationInView:self.view];
        // Граница ObjC: id касания — адрес UITouch, UIKit держит один объект на всё касание.
        const intptr_t id = reinterpret_cast<intptr_t>(t);
        game_->pointer(id, phase, static_cast<float>(p.x), static_cast<float>(p.y));
        if (probe_) probe_log(id, phase, p, game_->touch_state(), run_);
    }
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self dispatch:touches phase:mobile::Touch::Down];
}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self dispatch:touches phase:mobile::Touch::Move];
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self dispatch:touches phase:mobile::Touch::Up];
}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (started_) game_->cancel();
    [self dispatch:touches phase:mobile::Touch::Up];
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

@end
