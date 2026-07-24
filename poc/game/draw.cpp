#include "draw.hpp"

namespace game {
namespace {

void quad(SpriteBatch& b, float x, float y, float w, float h, const Region& r,
          float cr, float cg, float cb, float ca) {
    b.push({x, y, w, h, r.u0, r.v0, r.u1, r.v1, cr, cg, cb, ca});
}

// Число цифрами-спрайтами (MSB слева), левый край = x.
void push_number(SpriteBatch& b, const Atlas& atlas, uint32_t v, float x, float y,
                 float dw, float dh, float sp) {
    uint32_t digs[10];
    int n = 0;
    do { digs[n++] = v % 10u; v /= 10u; } while (v && n < 10);
    for (int i = 0; i < n; ++i)
        quad(b, x + i * sp, y, dw, dh, atlas.digit[digs[n - 1 - i]], 1, 1, 1, 1);
}

} // namespace

void push_scene(SpriteBatch& batch, flecs::world& world, const Atlas& atlas) {
    world.each([&](const Transform& t, const Star& s) {
        const float f = s.shade / 255.0f, sz = (float)s.size.to_double();
        quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), sz, sz, atlas.star,
             f * 0.85f, f * 0.92f, f, 1.0f);
    });
    world.each([&](const Transform& t, const Enemy&) {
        quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), 64, 48, atlas.enemy,
             1, 1, 1, 1);
    });
    world.each([&](flecs::entity e, const Transform& t, const Velocity&) {
        if (e.has<Bullet>())
            quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), 28, 10, atlas.bullet,
                 1, 1, 1, 1);
    });
    world.each([&](flecs::entity e, const Transform& t, const Velocity&) {
        if (e.has<Ship>())
            quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), 112, 76, atlas.ship,
                 1, 1, 1, 1);
    });
}

void push_hud(SpriteBatch& batch, const Atlas& atlas, const GameState& gs) {
    const float top = HALF_H - 26, left = -HALF_W + 22;
    push_number(batch, atlas, gs.score, left, top, 22, 28, 24);   // счёт слева-сверху
    const float rx = HALF_W - 78;                                 // жизни: мини-корабль + цифра
    quad(batch, rx, top + 2, 40, 27, atlas.ship, 1, 1, 1, 1);
    push_number(batch, atlas, gs.lives > 0 ? (uint32_t)gs.lives : 0u, rx + 34, top, 22, 28, 24);
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
