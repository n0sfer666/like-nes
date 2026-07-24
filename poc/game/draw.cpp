#include "draw.hpp"

#include <cstdio>
#include <cstring>

namespace game {
namespace {

void quad(SpriteBatch& b, float x, float y, float w, float h, const Region& r,
          float cr, float cg, float cb, float ca) {
    b.push({x, y, w, h, r.u0, r.v0, r.u1, r.v1, cr, cg, cb, ca});
}

const Region* glyph(const Atlas& a, char c) {
    if (c >= 'A' && c <= 'Z') return &a.letter[c - 'A'];
    if (c >= '0' && c <= '9') return &a.digit[c - '0'];
    return nullptr;   // пробел/прочее — только сдвиг
}

// Спрайт-текст: первый глиф центрирован в cx, шаг cw. Цвет — tint.
void push_text(SpriteBatch& b, const Atlas& a, const char* s, float cx, float y, float cw,
               float cr, float cg, float cb) {
    const float dw = cw * 0.72f, dh = cw * 1.05f;
    for (int i = 0; s[i]; ++i)
        if (const Region* g = glyph(a, s[i])) quad(b, cx + i * cw, y, dw, dh, *g, cr, cg, cb, 1);
}

void push_center(SpriteBatch& b, const Atlas& a, const char* s, float y, float cw,
                 float cr, float cg, float cb) {
    push_text(b, a, s, -((int)std::strlen(s) - 1) * cw * 0.5f, y, cw, cr, cg, cb);
}

} // namespace

void push_scene(SpriteBatch& batch, flecs::world& world, const Atlas& atlas) {
    world.each([&](const Transform& t, const Star& s) {
        const float f = s.shade / 255.0f, sz = (float)s.size.to_double();
        quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), sz, sz, atlas.star,
             f * 0.85f, f * 0.92f, f, 1.0f);
    });
    world.each([&](const Transform& t, const Boss&) {
        quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), 124, 92, atlas.boss, 1, 1, 1, 1);
    });
    world.each([&](const Transform& t, const Enemy&) {
        quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), 64, 48, atlas.enemy, 1, 1, 1, 1);
    });
    world.each([&](flecs::entity e, const Transform& t, const Velocity&) {
        if (e.has<Hostile>())
            quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), 26, 10, atlas.hostile, 1, 1, 1, 1);
        else if (e.has<Bullet>())
            quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), 28, 10, atlas.bullet, 1, 1, 1, 1);
    });
    world.each([&](flecs::entity e, const Transform& t, const Velocity&) {
        if (e.has<Ship>())
            quad(batch, (float)t.x.to_double(), (float)t.y.to_double(), 112, 76, atlas.ship, 1, 1, 1, 1);
    });
}

void push_hud(SpriteBatch& batch, flecs::world& world, const Atlas& atlas, const GameState& gs) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u", gs.score);
    push_text(batch, atlas, buf, -HALF_W + 34, HALF_H - 26, 24, 1, 1, 1);   // счёт слева
    const float rx = HALF_W - 82;                                           // жизни справа
    quad(batch, rx, HALF_H - 24, 40, 27, atlas.ship, 1, 1, 1, 1);
    std::snprintf(buf, sizeof(buf), "%d", gs.lives > 0 ? gs.lives : 0);
    push_text(batch, atlas, buf, rx + 36, HALF_H - 26, 24, 1, 1, 1);
    if (gs.phase == PH_Boss) {                                              // HP-бар босса сверху
        int32_t hp = 0;
        world.each([&](const Boss& b) { hp = b.hp; });
        const float w = 460, y = HALF_H - 12, frac = hp > 0 ? hp / (float)BOSS_HP_MAX : 0.0f;
        quad(batch, 0, y, w, 12, atlas.solid, 0.35f, 0.06f, 0.10f, 1);
        quad(batch, -w * 0.5f + w * frac * 0.5f, y, w * frac, 12, atlas.solid, 0.95f, 0.30f, 0.35f, 1);
    }
}

void push_screen(SpriteBatch& batch, const Atlas& atlas, const GameState& gs) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "SCORE %u", gs.score);
    if (gs.phase == PH_Intro) {
        push_center(batch, atlas, "LIKE NES", 120, 46, 0.85f, 0.92f, 1);
        push_center(batch, atlas, "SIDESCROLLER", 66, 22, 0.6f, 0.7f, 0.9f);
        push_center(batch, atlas, "DODGE AND SHOOT THE BOSS", -50, 16, 0.8f, 0.8f, 0.85f);
        push_center(batch, atlas, "PRESS FIRE TO START", -110, 20, 1, 0.8f, 0.3f);
    } else if (gs.phase == PH_Victory) {
        push_center(batch, atlas, "VICTORY", 100, 52, 0.4f, 1, 0.5f);
        push_center(batch, atlas, buf, 20, 26, 1, 1, 1);
        push_center(batch, atlas, "PRESS FIRE TO RESTART", -80, 18, 1, 0.8f, 0.3f);
    } else if (gs.phase == PH_GameOver) {
        push_center(batch, atlas, "GAME OVER", 100, 48, 1, 0.35f, 0.35f);
        push_center(batch, atlas, buf, 20, 26, 1, 1, 1);
        push_center(batch, atlas, "PRESS FIRE TO RESTART", -80, 18, 1, 0.8f, 0.3f);
    }
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
