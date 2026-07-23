#include "draw.hpp"

namespace game {

void push_scene(SpriteBatch& batch, flecs::world& world, const Atlas& atlas) {
    world.each([&](const Transform& t, const Star& s) {
        const float f = s.shade / 255.0f;
        const float sz = (float)s.size.to_double();
        batch.push({(float)t.x.to_double(), (float)t.y.to_double(), sz, sz,
                    atlas.star.u0, atlas.star.v0, atlas.star.u1, atlas.star.v1,
                    f * 0.85f, f * 0.92f, f, 1.0f});
    });
    world.each([&](const Transform& t, const Velocity&) {
        batch.push({(float)t.x.to_double(), (float)t.y.to_double(), 112.0f, 76.0f,
                    atlas.ship.u0, atlas.ship.v0, atlas.ship.u1, atlas.ship.v1,
                    1.0f, 1.0f, 1.0f, 1.0f});
    });
}

WGPURenderPassEncoder begin_clear(WGPUCommandEncoder enc, WGPUTextureView view) {
    WGPURenderPassColorAttachment a = {};
    a.view = view; a.loadOp = WGPULoadOp_Clear; a.storeOp = WGPUStoreOp_Store;
    a.clearValue = WGPUColor{0.02, 0.02, 0.07, 1.0};
    WGPURenderPassDescriptor d = {};
    d.colorAttachmentCount = 1; d.colorAttachments = &a;
    return wgpuCommandEncoderBeginRenderPass(enc, &d);
}

} // namespace game
