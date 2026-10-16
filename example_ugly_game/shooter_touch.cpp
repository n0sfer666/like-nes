#include "shooter_touch.hpp"

#include "codes.hpp"
#include "mobile_stick.hpp"

namespace game {
namespace {

struct Btn { float cx, cy, r; };

// Кнопка-огонь: правый-нижний угол, радиус/отступ как доля короткой стороны →
// вид (тач) и мир (рендер) пропорциональны одному экрану → совпадают визуально.
Btn fire_btn(float w, float h) {
    const float s = w < h ? w : h;
    const float r = 0.13f * s, m = 0.045f * s;
    return {w - m - r, h - m - r, r};
}

} // namespace

void ShooterTouch::bind(input::InputEngine* engine) {
    engine_ = engine;
    seq_ = 0;
    stick_id_ = fire_id_ = -1;
    fire_down_ = false;
    engine_->post({input::RawKind::DeviceConnected, input::DeviceKind::Gamepad, 0, 0, 0, seq_++});
}

void ShooterTouch::post_axis(fix32 x, fix32 y) {
    engine_->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                   static_cast<uint16_t>(input::code::LX), x.raw, seq_++});
    engine_->post({input::RawKind::PadAxis, input::DeviceKind::Gamepad, 0,
                   static_cast<uint16_t>(input::code::LY), y.raw, seq_++});
}

void ShooterTouch::post_fire(bool down) {
    fire_down_ = down;
    engine_->post({down ? input::RawKind::PadButtonDown : input::RawKind::PadButtonUp,
                   input::DeviceKind::Gamepad, 0, static_cast<uint16_t>(input::code::PadA), 0, seq_++});
}

void ShooterTouch::pointer(intptr_t id, mobile::Touch phase, float x, float y) {
    if (!engine_) return;
    const Btn b = fire_btn(w_, h_);
    const float stick_r = 0.12f * (w_ < h_ ? w_ : h_);
    if (phase == mobile::Touch::Down) {
        const float dx = x - b.cx, dy = y - b.cy;
        if (dx * dx + dy * dy <= b.r * b.r) {            // в круге кнопки → только огонь, не стик
            if (fire_id_ == -1) { fire_id_ = id; post_fire(true); }
        } else if (stick_id_ == -1) { stick_id_ = id; stick_ox_ = x; stick_oy_ = y; }
    } else if (phase == mobile::Touch::Move) {
        if (id == stick_id_) {
            const StickAxis a = stick_axis(x - stick_ox_, y - stick_oy_, stick_r);
            post_axis(a.x, a.y);
        }
    } else {
        if (id == fire_id_) { fire_id_ = -1; post_fire(false); }
        if (id == stick_id_) { stick_id_ = -1; post_axis(fix32{}, fix32{}); }
    }
}

void ShooterTouch::cancel() {
    if (!engine_) return;
    if (fire_id_ != -1) { fire_id_ = -1; post_fire(false); }
    if (stick_id_ != -1) { stick_id_ = -1; post_axis(fix32{}, fix32{}); }
}

void ShooterTouch::demo(uint32_t tick) {
    static const int DX[8] = {1, 0, -1, 0, 1, -1, -1, 1};
    static const int DY[8] = {0, -1, 0, 1, -1, -1, 1, 1};
    const uint32_t seg = (tick / 45) % 8;
    post_axis(fix32::from_int(DX[seg]), fix32::from_int(DY[seg]));
    if (tick == 60) post_fire(true);   // press → intro→play; удержан → непрерывный огонь → босс гибнет
}

void ShooterTouch::push_button(SpriteBatch& batch, const Region& star, float world_w, float world_h) const {
    const Btn b = fire_btn(world_w, world_h);
    const float cx = b.cx - world_w * 0.5f;    // вид (origin top-left, y вниз) → мир (центр, y вверх)
    const float cy = world_h * 0.5f - b.cy;
    const float d = b.r * 1.8f, k = fire_down_ ? 1.9f : 0.7f;
    batch.push({cx, cy, d, d, star.u0, star.v0, star.u1, star.v1,
                0.5f * k, 0.9f * k, 1.1f * k, fire_down_ ? 0.85f : 0.5f, 0});
}

} // namespace game
