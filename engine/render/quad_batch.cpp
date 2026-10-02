#include "quad_batch.hpp"

#include "quad_pipeline.hpp"

namespace render {
namespace {

WGPUBuffer make_buffer(WGPUDevice device, WGPUBufferUsageFlags usage, uint64_t size) {
    WGPUBufferDescriptor bd = {};
    bd.usage = usage | WGPUBufferUsage_CopyDst;
    bd.size = size;
    return wgpuDeviceCreateBuffer(device, &bd);
}

} // namespace

bool QuadRenderer::init(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat target,
                        uint32_t capacity) {
    shutdown();
    if (device == nullptr || queue == nullptr || capacity == 0) return false;
    device_ = device;
    queue_ = queue;
    capacity_ = capacity;
    texel_ = quad_texel_format(target);
    layout_ = make_quad_layout(device);
    pipeline_ = make_quad_pipeline(device, target, layout_);
    instances_ = make_buffer(device, WGPUBufferUsage_Vertex, uint64_t{capacity} * sizeof(Quad));
    screen_ = make_buffer(device, WGPUBufferUsage_Uniform, 16);
    return layout_ && pipeline_ && instances_ && screen_;
}

bool QuadRenderer::add_texture(const uint8_t* rgba, uint32_t w, uint32_t h) {
    if (layout_ == nullptr || screen_ == nullptr || rgba == nullptr || w == 0 || h == 0)
        return false;
    Texture t;
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{w, h, 1};
    td.format = texel_;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    t.texture = wgpuDeviceCreateTexture(device_, &td);
    if (t.texture == nullptr) return false;
    WGPUImageCopyTexture dst = {};
    dst.texture = t.texture;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = 4 * w;
    layout.rowsPerImage = h;
    const WGPUExtent3D ext = {w, h, 1};
    wgpuQueueWriteTexture(queue_, &dst, rgba, size_t{4} * w * h, &layout, &ext);
    t.view = wgpuTextureCreateView(t.texture, nullptr);
    WGPUBindGroupEntry b[2] = {};
    b[0].binding = 0; b[0].buffer = screen_; b[0].size = 16;
    b[1].binding = 1; b[1].textureView = t.view;
    WGPUBindGroupDescriptor bgd = {};
    bgd.layout = layout_; bgd.entryCount = 2; bgd.entries = b;
    t.group = wgpuDeviceCreateBindGroup(device_, &bgd);
    if (t.group == nullptr) {
        if (t.view) wgpuTextureViewRelease(t.view);
        wgpuTextureRelease(t.texture);
        return false;
    }
    textures_.push_back(t);
    return true;
}

bool QuadRenderer::upload(std::span<const Quad> quads, uint32_t screen_w, uint32_t screen_h) {
    uploaded_ = 0;
    if (device_ == nullptr || quads.size() > capacity_ || screen_w == 0 || screen_h == 0)
        return false;
    const float screen[4] = {static_cast<float>(screen_w), static_cast<float>(screen_h), 0, 0};
    wgpuQueueWriteBuffer(queue_, screen_, 0, screen, sizeof(screen));
    if (!quads.empty())
        wgpuQueueWriteBuffer(queue_, instances_, 0, quads.data(), quads.size_bytes());
    uploaded_ = static_cast<uint32_t>(quads.size());
    return true;
}

uint32_t QuadRenderer::draw(WGPURenderPassEncoder pass, std::span<const QuadRun> runs) const {
    if (pipeline_ == nullptr || uploaded_ == 0) return 0;
    wgpuRenderPassEncoderSetPipeline(pass, pipeline_);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, instances_, 0, uint64_t{uploaded_} * sizeof(Quad));
    uint32_t draws = 0;
    for (const QuadRun& r : runs) {
        if (r.count == 0 || r.texture >= textures_.size()) continue;
        if (r.first > uploaded_ || r.count > uploaded_ - r.first) continue;
        wgpuRenderPassEncoderSetBindGroup(pass, 0, textures_[r.texture].group, 0, nullptr);
        wgpuRenderPassEncoderDraw(pass, 6, r.count, 0, r.first);
        ++draws;
    }
    return draws;
}

void QuadRenderer::shutdown() {
    for (Texture& t : textures_) {
        if (t.group) wgpuBindGroupRelease(t.group);
        if (t.view) wgpuTextureViewRelease(t.view);
        if (t.texture) wgpuTextureRelease(t.texture);
    }
    textures_.clear();
    if (screen_) wgpuBufferRelease(screen_);
    if (instances_) wgpuBufferRelease(instances_);
    if (pipeline_) wgpuRenderPipelineRelease(pipeline_);
    if (layout_) wgpuBindGroupLayoutRelease(layout_);
    screen_ = instances_ = nullptr;
    pipeline_ = nullptr;
    layout_ = nullptr;
    device_ = nullptr;
    queue_ = nullptr;
    capacity_ = uploaded_ = 0;
    texel_ = WGPUTextureFormat_RGBA8Unorm;
}

} // namespace render
