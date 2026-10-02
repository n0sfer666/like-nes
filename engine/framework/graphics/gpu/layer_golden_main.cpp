#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "gpu.hpp"
#include "layer_golden.hpp"
#include "platform_args.hpp"
#include "render_capture.hpp"

// Гейт В5б спеки #24 на трёх ОС: слои `LNVL` через `QuadRenderer` против CPU-эталона, точно.
// Строки сверх вердикта — контроли: сломанный эталон обязан разойтись (сцена видит флипы), кадры
// соседних тиков — различаться (анимация доезжает до GPU), повтор — совпасть бит в бит.
namespace {

using namespace layer_golden;

constexpr uint32_t CAP = 1024;
Sprite storage[CAP];
uint64_t keys[CAP];
Batch batches[CAP];
render::Quad quads[CAP];
render::QuadRun runs[CAP];

int fails = 0;

void check(bool ok, const char* what) {
    if (ok) return;
    std::printf("layer-golden: FAIL: %s\n", what);
    ++fails;
}

uint32_t mismatched(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return W * H;
    uint32_t n = 0;
    for (std::size_t i = 0; i < a.size(); i += 4)
        if (std::memcmp(&a[i], &b[i], 4) != 0) ++n;
    return n;
}

double clear_channel(uint8_t c, bool srgb) {
    const double v = c / 255.0;
    if (!srgb) return v;
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

std::vector<uint8_t> render_frame(GpuContext& gpu, const render::QuadRenderer& qr, uint32_t n_runs,
                                  WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm) {
    const bool srgb = format == WGPUTextureFormat_RGBA8UnormSrgb;
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{W, H, 1};
    td.format = format;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    WGPUTexture tex = wgpuDeviceCreateTexture(gpu.device, &td);
    WGPUTextureView view = wgpuTextureCreateView(tex, nullptr);
    WGPUCommandEncoderDescriptor ed = {};
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, &ed);
    WGPURenderPassColorAttachment att = {};
    att.view = view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {clear_channel(CLEAR[0], srgb), clear_channel(CLEAR[1], srgb),
                      clear_channel(CLEAR[2], srgb), CLEAR[3] / 255.0};
    WGPURenderPassDescriptor pd = {};
    pd.colorAttachmentCount = 1;
    pd.colorAttachments = &att;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &pd);
    qr.draw(pass, {runs, n_runs});
    wgpuRenderPassEncoderEnd(pass);
    WGPUCommandBufferDescriptor cd = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, &cd);
    wgpuQueueSubmit(gpu.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(enc);
    std::vector<uint8_t> px = capture::readback_rgba(gpu.device, gpu.queue, tex, W, H);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(tex);
    return px;
}

struct Shot {
    std::vector<uint8_t> gpu;
    std::vector<uint8_t> cpu;
    std::vector<uint8_t> blind;
};

Shot shoot(GpuContext& gpu, render::QuadRenderer& qr, const Scene& s, const View& v) {
    SpriteList list(storage, keys, CAP);
    const Frame f = draw_view(s, v, list, batches, CAP);
    const QuadStats q = layer_quads(list, {batches, f.batches}, s.map, s.sizes, quads, runs);
    check(f.dropped == 0 && q.dropped == 0 && q.rejected == 0, "view fits the buffers");
    check(qr.upload({quads, q.quads}, W, H), "quads uploaded");
    Shot out{render_frame(gpu, qr, q.runs), reference(s, list, true), reference(s, list, false)};
    const uint32_t bad = mismatched(out.gpu, out.cpu);
    std::printf("layer-golden: %s tick %llu zoom %d: %u sprite(s), %u run(s), %u mismatched\n",
                v.name, static_cast<unsigned long long>(v.tick), v.zoom, f.sprites, q.runs, bad);
    check(bad == 0, "GPU frame equals the CPU reference");
    return out;
}

void quads_refuse(const Scene& s) {
    SpriteList list(storage, keys, CAP);
    const Frame f = draw_view(s, VIEWS[0], list, batches, CAP);
    TextureSize small[TEXTURES] = {s.sizes[0], s.sizes[1], s.sizes[2]};
    small[0].w = 64;
    const QuadStats r = layer_quads(list, {batches, f.batches}, s.map, small, quads, runs);
    const QuadStats d = layer_quads(list, {batches, f.batches}, s.map, s.sizes, quads, {runs, 1});
    std::printf("layer-golden: quads: %u rejected past a narrowed texture, %u dropped past 1 run\n",
                r.rejected, d.dropped);
    check(r.rejected > 0 && d.dropped > 0 && d.runs == 1, "quads refuse what does not fit");
}

// Окно на Vulkan и DX12 получает sRGB-поверхность (`formats[0]`): тот же кадр на sRGB-цели обязан
// лечь теми же байтами, иначе тайлы в окне светлее, чем в Tiled, а голден на RGBA8Unorm этого не видит.
uint32_t srgb_target(GpuContext& gpu, const Scene& s, const std::vector<uint8_t>& unorm) {
    render::QuadRenderer qs;
    check(qs.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA8UnormSrgb, CAP), "sRGB renderer");
    for (const Image& im : s.textures) check(qs.add_texture(im.rgba.data(), im.w, im.h), "texture");
    SpriteList list(storage, keys, CAP);
    const Frame f = draw_view(s, VIEWS[0], list, batches, CAP);
    const QuadStats q = layer_quads(list, {batches, f.batches}, s.map, s.sizes, quads, runs);
    check(qs.upload({quads, q.quads}, W, H), "quads uploaded");
    const uint32_t bad =
        mismatched(unorm, render_frame(gpu, qs, q.runs, WGPUTextureFormat_RGBA8UnormSrgb));
    std::printf("layer-golden: srgb: %u pixel(s) differ on an sRGB target\n", bad);
    return bad;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const char* out_dir = nullptr;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--out") == 0) out_dir = argv[i + 1];
    Scene s;
    if (!build_scene(s)) {
        std::printf("layer-golden: FAIL: scene does not bake\n");
        return 1;
    }
    quads_refuse(s);
    GpuContext gpu;
    if (!gpu.init(nullptr)) {
        std::printf("layer-golden: FAIL: no GPU adapter\n");
        return 1;
    }
    render::QuadRenderer qr;
    if (!qr.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA8Unorm, CAP)) {
        std::printf("layer-golden: FAIL: quad renderer\n");
        qr.shutdown();
        gpu.shutdown();
        return 1;
    }
    for (const Image& im : s.textures) check(qr.add_texture(im.rgba.data(), im.w, im.h), "texture");
    Shot shots[3];
    for (uint32_t i = 0; i < 3; ++i) {
        shots[i] = shoot(gpu, qr, s, VIEWS[i]);
        if (out_dir != nullptr)
            capture::write_png((std::string(out_dir) + "/layer_" + VIEWS[i].name + ".png").c_str(),
                               shots[i].gpu, W, H);
    }
    const uint32_t blind = mismatched(shots[0].gpu, shots[0].blind);
    const uint32_t anim = mismatched(shots[0].gpu, shots[1].gpu);
    const uint32_t repeat = mismatched(shots[0].gpu, shoot(gpu, qr, s, VIEWS[0]).gpu);
    std::printf("layer-golden: control: flip-blind reference %u mismatched\n", blind);
    std::printf("layer-golden: anim: ticks 2 and 3 differ in %u pixel(s)\n", anim);
    std::printf("layer-golden: repeat: %u pixel(s) differ between two runs\n", repeat);
    check(blind > 0, "flip-blind reference diverges");
    check(anim > 0 && anim <= 2 * 16 * 16, "only the animated cells change between ticks");
    check(repeat == 0, "two runs agree");
    check(srgb_target(gpu, s, shots[0].gpu) == 0, "sRGB target keeps the texel bytes");
    qr.shutdown();
    gpu.shutdown();
    if (fails != 0) return 1;
    std::printf("layer-golden: PASS\n");
    return 0;
}
