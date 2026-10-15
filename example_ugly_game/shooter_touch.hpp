#pragma once
#include <cstdint>

#include "art.hpp"
#include "batch.hpp"
#include "fixed.hpp"
#include "input_engine.hpp"
#include "mobile_app.hpp"

namespace game {

// Тач шутера: левая зона = виртуальный стик (движение), правый-нижний круг = огонь (PadA). Касания
// уходят в движок ввода событиями пада, и симуляция не отличает экран от настоящего геймпада.
// Координаты и размер вида — в точках: в пикселях кнопка была бы разной на экранах разной плотности.
class ShooterTouch {
public:
    void bind(input::InputEngine* engine);
    void set_view(float w_pt, float h_pt) { w_ = w_pt; h_ = h_pt; }
    void pointer(intptr_t id, mobile::Touch phase, float x, float y);
    void cancel();
    void demo(uint32_t tick);
    bool idle() const { return stick_id_ == -1 && fire_id_ == -1; }
    // Чьё касание держит стик и огонь (-1 — ничьё): гейт симулятора судит маршрутизацию по ним.
    intptr_t stick() const { return stick_id_; }
    intptr_t fire() const { return fire_id_; }
    void push_button(SpriteBatch& batch, const Region& star, float world_w, float world_h) const;

private:
    void post_axis(fix32 x, fix32 y);
    void post_fire(bool down);

    input::InputEngine* engine_ = nullptr;
    uint64_t seq_ = 0;
    float w_ = 0, h_ = 0;
    intptr_t stick_id_ = -1, fire_id_ = -1;
    float stick_ox_ = 0, stick_oy_ = 0;
    bool fire_down_ = false;
};

} // namespace game
