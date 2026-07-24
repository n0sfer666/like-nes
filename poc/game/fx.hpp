#pragma once
#include <cstdint>
#include <vector>

#include "batch.hpp"
#include "fx_events.hpp"
#include "world.hpp"

namespace game {

// Частица (чисто презентационная, float). Рендерится soft-glow спрайтом (star) с fade+rotate;
// bloom добавляет свечение.
struct Particle { float x, y, vx, vy, life, life0, size, r, g, b, rot, vrot; };

class Fx {
public:
    void emit(const FxSink& sink);                 // взрывы/искры/вспышки из событий
    void emit_trails(flecs::world& world);         // выхлоп корабля + пуль (каждый кадр)
    void update(float dt);
    void render(SpriteBatch& batch, const Atlas& atlas) const;
    void clear() { ps_.clear(); }

private:
    void burst(float x, float y, int n, float spd, float r, float g, float b, float size);
    std::vector<Particle> ps_;
    uint32_t rng_ = 0x2545f491u;
};

} // namespace game
