#include "fx.hpp"

#include <cmath>

namespace game {
namespace {
constexpr float PI2 = 6.2831853f;
}

float Fx_frand(uint32_t& s) { s = s * 1664525u + 1013904223u; return (s >> 8) * (1.0f / 16777216.0f); }

void Fx::burst(float x, float y, int n, float spd, float r, float g, float b, float size) {
    for (int i = 0; i < n; ++i) {
        const float a = Fx_frand(rng_) * PI2;
        const float v = spd * (0.35f + 0.65f * Fx_frand(rng_));
        const float life = 0.28f + 0.55f * Fx_frand(rng_);
        ps_.push_back({x, y, std::cos(a) * v, std::sin(a) * v, life, life,
                       size * (0.6f + 0.7f * Fx_frand(rng_)), r, g, b,
                       Fx_frand(rng_) * PI2, (Fx_frand(rng_) - 0.5f) * 12.0f});
    }
}

void Fx::emit(const FxSink& sink) {
    for (const FxEvent& e : sink.events) {
        switch (e.kind) {
            case FX_Fire:     burst(e.x, e.y, 4, 120, 0.65f, 0.92f, 1.0f, 7); break;
            case FX_EnemyDie: burst(e.x, e.y, 16, 250, 1.0f, 0.62f, 0.22f, 12); break;
            case FX_BossHit:  burst(e.x, e.y, 5, 170, 1.0f, 0.5f, 0.92f, 8); break;
            case FX_BossDie:  burst(e.x, e.y, 46, 340, 1.0f, 0.72f, 0.32f, 18);
                              burst(e.x, e.y, 14, 140, 1.0f, 1.0f, 0.9f, 22); break;
            case FX_PlayerHit: burst(e.x, e.y, 12, 220, 1.0f, 0.32f, 0.32f, 11); break;
        }
    }
}

void Fx::emit_trails(flecs::world& world) {
    world.each([&](flecs::entity e, const Transform& t, const Velocity&) {
        const float x = (float)t.x.to_double(), y = (float)t.y.to_double();
        if (e.has<Ship>()) {
            const float j = (Fx_frand(rng_) - 0.5f) * 10.0f;
            ps_.push_back({x - 52, y + j, -90 - Fx_frand(rng_) * 60, j * 2, 0.34f, 0.34f,
                           9, 0.55f, 0.85f, 1.0f, 0, 0});
        } else if (e.has<Bullet>()) {
            ps_.push_back({x - 16, y, -60, 0, 0.16f, 0.16f, 7, 0.6f, 0.95f, 1.0f, 0, 0});
        }
    });
}

void Fx::update(float dt) {
    size_t w = 0;
    for (Particle& p : ps_) {
        p.life -= dt;
        if (p.life <= 0) continue;
        p.x += p.vx * dt; p.y += p.vy * dt;
        p.vx *= 0.90f; p.vy *= 0.90f;
        p.rot += p.vrot * dt;
        ps_[w++] = p;
    }
    ps_.resize(w);
}

void Fx::render(SpriteBatch& batch, const Atlas& atlas) const {
    for (const Particle& p : ps_) {
        const float k = p.life / p.life0;              // 1→0 fade
        const float sz = p.size * (0.4f + 0.6f * k);
        const float br = 1.9f;                          // яркость >1 → bloom-свечение
        batch.push({p.x, p.y, sz, sz, atlas.star.u0, atlas.star.v0, atlas.star.u1, atlas.star.v1,
                    p.r * br, p.g * br, p.b * br, k, p.rot});
    }
}

} // namespace game
