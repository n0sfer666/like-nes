#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <webgpu/webgpu.h>

#include "art.hpp"
#include "batch.hpp"
#include "draw.hpp"
#include "gpu.hpp"
#include "sim.hpp"
#include "stick.hpp"
#include "world.hpp"
#include "action_map.hpp"
#include "codes.hpp"
#include "engine.hpp"

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
    SpriteBatch batch_;
    Atlas atlas_;
    flecs::world world_;
    input::ActionMap map_;
    input::InputEngine* engine_;
    WGPUSurface surface_;
    CADisplayLink* link_;
    uint32_t tick_;
    uint64_t seq_;
    CGPoint origin_;
    bool tracking_;
    bool demo_;
}
@end

@implementation GameViewController

- (void)loadView {
    self.view = [[MetalView alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.view.multipleTouchEnabled = NO;
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if (engine_) return;

    CAMetalLayer* layer = (CAMetalLayer*)self.view.layer;
    const CGFloat scale = UIScreen.mainScreen.scale;
    layer.contentsScale = scale;
    const uint32_t w = (uint32_t)(self.view.bounds.size.width * scale);
    const uint32_t h = (uint32_t)(self.view.bounds.size.height * scale);
    layer.drawableSize = CGSizeMake(w, h);

    gpu_.instance = wgpuCreateInstance(nullptr);
    WGPUSurfaceDescriptorFromMetalLayer ml = {};
    ml.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
    ml.layer = (__bridge void*)layer;
    WGPUSurfaceDescriptor sd = {};
    sd.nextInChain = &ml.chain;
    surface_ = wgpuInstanceCreateSurface(gpu_.instance, &sd);
    if (!gpu_.init(surface_)) { NSLog(@"[game] gpu init failed"); return; }

    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(surface_, gpu_.adapter, &caps);
    WGPUTextureFormat fmt = caps.formatCount ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration cfg = {};
    cfg.device = gpu_.device; cfg.format = fmt;
    cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
    cfg.width = w; cfg.height = h; cfg.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface_, &cfg);
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    atlas_ = build_atlas();
    batch_.init(gpu_.device, gpu_.queue, fmt, atlas_);
    const double sa = (double)w / (double)h, wa = (double)VIEW_W / (double)VIEW_H;
    const uint32_t vw = (sa >= wa) ? (uint32_t)(VIEW_H * sa) : VIEW_W;
    const uint32_t vh = (sa >= wa) ? VIEW_H : (uint32_t)(VIEW_W / sa);
    batch_.set_viewport(vw, vh);
    spawn(world_);
    map_ = make_map();
    engine_ = new input::InputEngine(map_);
    engine_->post({input::RawKind::DeviceConnected, input::DeviceKind::Gamepad, 0, 0, 0, seq_++});
    demo_ = [NSProcessInfo.processInfo.arguments containsObject:@"--demo"];
    NSLog(@"[game] iOS shell up: %ux%u — touch left-bottom = virtual stick", w, h);

    link_ = [CADisplayLink displayLinkWithTarget:[WeakProxy with:self] selector:@selector(frame)];
    [link_ addToRunLoop:NSRunLoop.mainRunLoop forMode:NSDefaultRunLoopMode];

    NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
    [nc addObserver:self selector:@selector(pause) name:UIApplicationDidEnterBackgroundNotification object:nil];
    [nc addObserver:self selector:@selector(resume) name:UIApplicationWillEnterForegroundNotification object:nil];
}

- (void)pause { link_.paused = YES; }
- (void)resume { link_.paused = NO; }

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [link_ invalidate];
    delete engine_;
    batch_.shutdown();
    if (surface_) wgpuSurfaceRelease(surface_);
    gpu_.shutdown();
}

- (void)frame {
    if (demo_ && !tracking_) {
        static const int DX[8] = {1, 0, -1, 0, 1, -1, -1, 1};
        static const int DY[8] = {0, -1, 0, 1, -1, -1, 1, 1};
        const int seg = (tick_ / 45) % 8;
        [self emitAxis:fix32::from_int(DX[seg]) y:fix32::from_int(DY[seg])];
    }
    const fix32 dt = fix32::from_float(1.0 / 60);
    const input::InputFrame& f = engine_->begin_tick(tick_++, 0);
    step(world_, f, dt);

    WGPUSurfaceTexture st = {};
    wgpuSurfaceGetCurrentTexture(surface_, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        if (st.texture) wgpuTextureRelease(st.texture);
        return;
    }
    WGPUTextureView view = wgpuTextureCreateView(st.texture, nullptr);
    batch_.begin();
    push_scene(batch_, world_, atlas_);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu_.device, nullptr);
    WGPURenderPassEncoder pass = begin_clear(enc, view);
    batch_.flush(pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(gpu_.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
    wgpuSurfacePresent(surface_);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(st.texture);
}

- (void)emitAxis:(fix32)x y:(fix32)y {
    engine_->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                   (uint16_t)input::code::LX, x.raw, seq_++});
    engine_->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                   (uint16_t)input::code::LY, y.raw, seq_++});
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (!engine_) return;
    origin_ = [touches.anyObject locationInView:self.view];
    tracking_ = true;
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (!engine_ || !tracking_) return;
    const CGPoint p = [touches.anyObject locationInView:self.view];
    const StickAxis a = stick_axis(p.x - origin_.x, p.y - origin_.y, 56.0);
    [self emitAxis:a.x y:a.y];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    tracking_ = false;
    if (engine_) [self emitAxis:fix32{} y:fix32{}];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    tracking_ = false;
    if (engine_) [self emitAxis:fix32{} y:fix32{}];
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskPortrait;
}

@end
