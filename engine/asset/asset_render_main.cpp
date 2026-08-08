#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include <chrono>
#include <thread>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "asset_manager.hpp"
#include "capture.hpp"
#include "gpu.hpp"
#include "hash.hpp"
#include "platform_args.hpp"
#include "transcode.hpp"

// Шов asset→render (спека #5 validation gate): реальный бейкнутый ассет кормит GPU —
// baked Tint SPIR-V → WGPUShaderModule (zero-copy из mmap), baked KTX2 → BC7 транскод →
// текстура. Доказывает шов end-to-end и валидирует SPIR-V-ingestion (naga→Metal, спека #2).

using namespace asset;

namespace {

uint64_t guid_of(const char* n) { return fnv1a(n, std::strlen(n)); }

WGPUShaderModule spirv_module(WGPUDevice dev, const Loaded& spv) {
    WGPUShaderModuleSPIRVDescriptor sd = {};
    sd.chain.sType = WGPUSType_ShaderModuleSPIRVDescriptor;
    sd.codeSize = spv.size / 4; // в u32-словах
    sd.code = reinterpret_cast<const uint32_t*>(spv.data);
    WGPUShaderModuleDescriptor md = {};
    md.nextInChain = &sd.chain;
    return wgpuDeviceCreateShaderModule(dev, &md);
}

WGPUTexture upload_bc7(WGPUDevice dev, WGPUQueue q, const std::vector<uint8_t>& bc7,
                       uint32_t w, uint32_t h, WGPUTextureView* view) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = {w, h, 1};
    td.format = WGPUTextureFormat_BC7RGBAUnorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    WGPUTexture tex = wgpuDeviceCreateTexture(dev, &td);
    WGPUImageCopyTexture dst = {};
    dst.texture = tex;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = ((w + 3) / 4) * 16; // BC7: 16 байт/4x4-блок
    layout.rowsPerImage = (h + 3) / 4;
    WGPUExtent3D ext = {w, h, 1};
    wgpuQueueWriteTexture(q, &dst, bc7.data(), bc7.size(), &layout, &ext);
    *view = wgpuTextureCreateView(tex, nullptr);
    return tex;
}

int fail(const char* m) { std::fprintf(stderr, "[asset_render] FAIL: %s\n", m); return 1; }

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 2) return fail("usage: asset_render <bundle> [out.png]");
    const std::string bundle = argv[1];
    const char* out_png = argc >= 3 ? argv[2] : nullptr;
    constexpr uint32_t W = 256, H = 256;

    GpuContext gpu;
    if (!gpu.init(nullptr)) return fail("gpu init (headless)");

    AssetManager am;
    if (!am.open(bundle, 8u * 1024 * 1024, /*trusted=*/false)) return fail("open bundle");
    const uint64_t g_vs = guid_of("sprite.vs"), g_fs = guid_of("sprite.fs"),
                   g_alb = guid_of("hero_albedo");
    am.request(g_vs); am.request(g_fs); am.request(g_alb);
    for (int f = 0; f < 500; ++f) {
        am.sync_point();
        if (am.is_ready(g_vs) && am.is_ready(g_fs) && am.is_ready(g_alb)) break;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    if (!am.is_ready(g_vs) || !am.is_ready(g_fs) || !am.is_ready(g_alb))
        return fail("assets not ready");

    Loaded vs = am.get(g_vs), fs = am.get(g_fs), alb = am.get(g_alb);
    if (!vs.zero_copy || !fs.zero_copy) return fail("shader not zero-copy from mmap");

    // KTX2 (staged в арене) → BC7 транскод.
    std::vector<uint8_t> bc7; uint32_t tw = 0, th = 0;
    if (!ktx2_to_bc7(alb.data, alb.size, bc7, tw, th)) return fail("ktx2->bc7 transcode");

    WGPUShaderModule vsm = spirv_module(gpu.device, vs);
    WGPUShaderModule fsm = spirv_module(gpu.device, fs);
    if (!vsm || !fsm) return fail("spirv shader module (naga ingest)");
    WGPUTextureView texv = nullptr;
    WGPUTexture tex = upload_bc7(gpu.device, gpu.queue, bc7, tw, th, &texv);

    WGPUSamplerDescriptor smp = {};
    smp.magFilter = WGPUFilterMode_Linear; smp.minFilter = WGPUFilterMode_Linear;
    smp.addressModeU = WGPUAddressMode_ClampToEdge; smp.addressModeV = WGPUAddressMode_ClampToEdge;
    smp.addressModeW = WGPUAddressMode_ClampToEdge; smp.maxAnisotropy = 1;
    WGPUSampler sampler = wgpuDeviceCreateSampler(gpu.device, &smp);

    const float uni[4] = {0.0f, 0.0f, 1.7f, 1.7f}; // offset.xy, scale.xy
    WGPUBufferDescriptor ubd = {};
    ubd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst; ubd.size = sizeof(uni);
    WGPUBuffer ubo = wgpuDeviceCreateBuffer(gpu.device, &ubd);
    wgpuQueueWriteBuffer(gpu.queue, ubo, 0, uni, sizeof(uni));

    WGPUBindGroupLayoutEntry bgle[3] = {};
    bgle[0].binding = 0; bgle[0].visibility = WGPUShaderStage_Fragment;
    bgle[0].sampler.type = WGPUSamplerBindingType_Filtering;
    bgle[1].binding = 1; bgle[1].visibility = WGPUShaderStage_Fragment;
    bgle[1].texture.sampleType = WGPUTextureSampleType_Float;
    bgle[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    bgle[2].binding = 2; bgle[2].visibility = WGPUShaderStage_Vertex;
    bgle[2].buffer.type = WGPUBufferBindingType_Uniform;
    WGPUBindGroupLayoutDescriptor bgld = {}; bgld.entryCount = 3; bgld.entries = bgle;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(gpu.device, &bgld);

    WGPUBindGroupEntry bge[3] = {};
    bge[0].binding = 0; bge[0].sampler = sampler;
    bge[1].binding = 1; bge[1].textureView = texv;
    bge[2].binding = 2; bge[2].buffer = ubo; bge[2].size = sizeof(uni);
    WGPUBindGroupDescriptor bgd = {}; bgd.layout = bgl; bgd.entryCount = 3; bgd.entries = bge;
    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(gpu.device, &bgd);

    WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(gpu.device, &pld);

    WGPUColorTargetState color = {}; color.format = WGPUTextureFormat_RGBA8Unorm;
    color.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState frag = {}; frag.module = fsm; frag.entryPoint = "fs_main";
    frag.targetCount = 1; frag.targets = &color;
    WGPURenderPipelineDescriptor rpd = {};
    rpd.layout = pl;
    rpd.vertex.module = vsm; rpd.vertex.entryPoint = "vs_main";
    rpd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rpd.multisample.count = 1; rpd.multisample.mask = 0xFFFFFFFF;
    rpd.fragment = &frag;
    WGPURenderPipeline pipe = wgpuDeviceCreateRenderPipeline(gpu.device, &rpd);
    if (!pipe) return fail("render pipeline (SPIR-V->pipeline)");

    // Offscreen RGBA8 → рендер quad → readback.
    WGPUTextureDescriptor otd = {};
    otd.dimension = WGPUTextureDimension_2D; otd.size = {W, H, 1};
    otd.format = WGPUTextureFormat_RGBA8Unorm; otd.mipLevelCount = 1; otd.sampleCount = 1;
    otd.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    WGPUTexture ot = wgpuDeviceCreateTexture(gpu.device, &otd);
    WGPUTextureView ov = wgpuTextureCreateView(ot, nullptr);

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
    WGPURenderPassColorAttachment att = {};
    att.view = ov; att.loadOp = WGPULoadOp_Clear; att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {0.05, 0.05, 0.08, 1.0};
    WGPURenderPassDescriptor rp = {}; rp.colorAttachmentCount = 1; rp.colorAttachments = &att;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
    wgpuRenderPassEncoderSetPipeline(pass, pipe);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bg, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 6, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(gpu.queue, 1, &cmd);

    std::vector<uint8_t> rgba = capture::readback_rgba(gpu.device, gpu.queue, ot, W, H);
    if (rgba.empty()) return fail("readback");

    // Sanity: спрайт реально просэмплен (не только clear-фон) — доля непрозрачных пикселей.
    size_t lit = 0;
    for (size_t p = 0; p + 3 < rgba.size(); p += 4)
        if (rgba[p] > 40 || rgba[p + 1] > 40 || rgba[p + 2] > 40) ++lit;
    const double frac = static_cast<double>(lit) / (W * H);
    if (out_png) capture::write_png(out_png, rgba, W, H);
    std::printf("[asset_render] PASS seam: SPIR-V->pipeline + BC7 tex %ux%u, lit_frac=%.3f%s%s\n",
                tw, th, frac, out_png ? " png=" : "", out_png ? out_png : "");

    wgpuRenderPassEncoderRelease(pass); wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
    wgpuTextureViewRelease(ov); wgpuTextureRelease(ot);
    wgpuRenderPipelineRelease(pipe); wgpuPipelineLayoutRelease(pl);
    wgpuBindGroupRelease(bg); wgpuBindGroupLayoutRelease(bgl); wgpuBufferRelease(ubo);
    wgpuSamplerRelease(sampler); wgpuTextureViewRelease(texv); wgpuTextureRelease(tex);
    wgpuShaderModuleRelease(vsm); wgpuShaderModuleRelease(fsm);
    am.close();
    return frac > 0.20 ? 0 : fail("sprite not visible (seam)");
}
