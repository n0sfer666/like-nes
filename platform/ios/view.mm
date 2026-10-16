#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <webgpu/webgpu.h>

#include <memory>

#include "gpu.hpp"
#include "mobile_app.hpp"

#import "metal_view.h"
#import "weak_proxy.h"

// Оболочка не знает, какую игру держит: всё игровое — за mobile::Game из make_game().
@interface GameViewController : UIViewController {
    GpuContext gpu_;
    std::unique_ptr<mobile::Game> game_;
    WGPUSurface surface_;
    CADisplayLink* link_;
    CFTimeInterval last_;
    uint32_t fb_w_, fb_h_;
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

- (mobile::SafeArea)safeArea {
    const UIEdgeInsets in = self.view.safeAreaInsets;
    return {static_cast<float>(in.left), static_cast<float>(in.top), static_cast<float>(in.right),
            static_cast<float>(in.bottom), static_cast<float>(self.traitCollection.displayScale)};
}

// Размер кадра — в пикселях слоя: точки вида × displayScale.
- (bool)fitLayer {
    CAMetalLayer* layer = static_cast<CAMetalLayer*>(self.view.layer);
    const CGFloat scale = self.traitCollection.displayScale;
    const uint32_t w = static_cast<uint32_t>(self.view.bounds.size.width * scale);
    const uint32_t h = static_cast<uint32_t>(self.view.bounds.size.height * scale);
    const bool changed = w != fb_w_ || h != fb_h_;
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(w, h);
    fb_w_ = w; fb_h_ = h;
    return changed;
}

- (bool)makeSurface {
    [self fitLayer];
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
    if (!game_->init(gpu_) || !game_->attach_surface(surface_, fb_w_, fb_h_, [self safeArea])) {
        NSLog(@"[game] game init failed");
        [self teardown];
        return;
    }
    probe_ = [NSProcessInfo.processInfo.arguments containsObject:@"--touch-probe"];
    run_ = NSProcessInfo.processInfo.environment[@"LIKE_NES_TOUCH_RUN"] ?: @"-";
    started_ = true;
    NSLog(@"[game] iOS shell up: %ux%u", fb_w_, fb_h_);

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
    game_->suspend();
    game_->detach_surface();
    [self releaseSurface];
}

- (void)resume {
    if (!started_ || surface_) return;
    if (![self makeSurface] || !game_->attach_surface(surface_, fb_w_, fb_h_, [self safeArea])) {
        NSLog(@"[game] surface attach failed");
        [self releaseSurface];
        return;
    }
    game_->resume();
    last_ = 0;
    [self pace];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    if (!started_ || !surface_ || ![self fitLayer]) return;
    game_->resize(fb_w_, fb_h_, [self safeArea]);
    [self pace];
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
        if (probe_) [self probe:id phase:phase at:p];
    }
}

// Строка на касание с состоянием ПОСЛЕ него: XCUITest проигрывает жест целиком, и гейт
// (`scripts/ios_sim_gate.sh`) судит историю из системного лога. id32 — усечение до int, каким id был
// до B8: улика, что оно не склеивает касания. run — нонс прогона: чужие строки вердикт отбрасывает.
- (void)probe:(intptr_t)id phase:(mobile::Touch)phase at:(CGPoint)p {
    static const char* const kPhase[] = {"down", "move", "up"};
    NSLog(@"[touch] %s id=%ld id32=%d x=%.0f y=%.0f %s run=%@", kPhase[static_cast<int>(phase)],
          static_cast<long>(id), static_cast<int32_t>(id), p.x, p.y, game_->touch_state().c_str(), run_);
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
    [self dispatch:touches phase:mobile::Touch::Up];
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskPortrait;
}

@end
