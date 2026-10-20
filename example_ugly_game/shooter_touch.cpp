#include "shooter_touch.hpp"

#include <cstdio>
#include <optional>
#include <vector>

#include "bundle_asset.hpp"
#include "codes.hpp"

namespace game {
namespace {

using framework::touch::Circle;
using framework::touch::Control;
using framework::Vec2;

fix32 pt(float v) { return fix32::from_float(v); }
float fl(fix32 v) { return static_cast<float>(v.to_double()); }

framework::touch::Phase to_phase(mobile::Touch t) {
    switch (t) {
    case mobile::Touch::Down: return framework::touch::Phase::Down;
    case mobile::Touch::Move: return framework::touch::Phase::Move;
    case mobile::Touch::Up: return framework::touch::Phase::Up;
    }
    return framework::touch::Phase::Up;
}

struct View { float k, half_w, half_h; };

// Вид — origin слева сверху, y вниз, точки; мир — центр, y вверх. Аспекты у них равны, поэтому
// масштаб один на обе оси.
void sprite(SpriteBatch& batch, const Region& star, const View& v, Vec2 at, float d, float glow, float alpha) {
    const float cx = fl(at.x) * v.k - v.half_w;
    const float cy = v.half_h - fl(at.y) * v.k;
    batch.push({cx, cy, d * v.k, d * v.k, star.u0, star.v0, star.u1, star.v1,
                0.5f * glow, 0.9f * glow, 1.1f * glow, alpha, 0});
}

} // namespace

bool ShooterTouch::load(const std::string& bundle_path) {
    std::vector<uint8_t> bytes;
    framework::touch::Layout layout;
    if (!read_bundle_asset(bundle_path, "touch", bytes) ||
        !framework::touch::read_layout(bytes.data(), bytes.size(), layout)) {
        std::fprintf(stderr, "[game] touch: no valid touch layout in bundle\n");
        return false;
    }
    std::string why;
    if (!framework::touch::layout_ok(layout, why) || !source_.set_layout(layout)) {
        std::fprintf(stderr, "[game] touch: layout refused: %s\n", why.c_str());
        return false;
    }
    return true;
}

void ShooterTouch::bind(input::InputEngine* engine) {
    engine_ = engine;
    pad_seq_ = PAD_SEQ_BASE;
    pad_connected_ = pad_moving_ = pad_a_ = false;
    source_.bind(engine);
}

void ShooterTouch::set_view(float w_pt, float h_pt, const mobile::SafeArea& safe) {
    w_ = w_pt;
    source_.set_view(pt(w_pt), pt(h_pt), {pt(safe.left), pt(safe.top), pt(safe.right), pt(safe.bottom)});
}

void ShooterTouch::pointer(intptr_t id, mobile::Touch phase, float x_pt, float y_pt) {
    source_.pointer(id, to_phase(phase), pt(x_pt), pt(y_pt));
}

void ShooterTouch::post_pad(input::RawKind kind, uint16_t code, int32_t value) {
    if (engine_ == nullptr) return;
    engine_->post({kind, input::DeviceKind::Gamepad, 0, code, value, pad_seq_++});
}

// Пад и тач — два источника одного места, и карта берёт последний ненулевой: пад, брошенный демо
// под пальцем, вёл бы корабль своим наклоном в мёртвой зоне стика и держал бы огонь после отпускания
// кнопки. Поэтому палец на экране сначала отпускает пад.
void ShooterTouch::yield_pad() {
    if (pad_moving_) {
        pad_moving_ = false;
        post_pad(input::RawKind::PadAxis, static_cast<uint16_t>(input::code::LX), 0);
        post_pad(input::RawKind::PadAxis, static_cast<uint16_t>(input::code::LY), 0);
    }
    if (pad_a_) {
        pad_a_ = false;
        post_pad(input::RawKind::PadButtonUp, static_cast<uint16_t>(input::code::PadA), 0);
    }
}

// Демо играет падом, а не экраном: оно должно пройти шутер и на прогоне без пальца, и в CI-гейтах
// сборки, где касаний нет, а пад — тот же путь пресета.
void ShooterTouch::demo(uint32_t tick) {
    static const int DX[8] = {1, 0, -1, 0, 1, -1, -1, 1};
    static const int DY[8] = {0, -1, 0, 1, -1, -1, 1, 1};
    if (!source_.idle()) {
        yield_pad();
        return;
    }
    if (!pad_connected_) {
        pad_connected_ = true;
        post_pad(input::RawKind::DeviceConnected, 0, 0);
    }
    const uint32_t seg = (tick / 45) % 8;
    pad_moving_ = true;
    post_pad(input::RawKind::PadAxis, static_cast<uint16_t>(input::code::LX), fix32::from_int(DX[seg]).raw);
    post_pad(input::RawKind::PadAxis, static_cast<uint16_t>(input::code::LY), fix32::from_int(DY[seg]).raw);
    if (tick >= 60 && !pad_a_) {
        pad_a_ = true;
        post_pad(input::RawKind::PadButtonDown, static_cast<uint16_t>(input::code::PadA), 0);
    }
}

std::string ShooterTouch::state() const {
    const std::optional<intptr_t> stick = source_.owner(Control::Stick), a = source_.owner(Control::A);
    char buf[64];
    std::snprintf(buf, sizeof buf, "stick=%ld a=%ld", static_cast<long>(stick.value_or(-1)),
                  static_cast<long>(a.value_or(-1)));
    return buf;
}

void ShooterTouch::push(SpriteBatch& batch, const Region& star, float world_w, float world_h) const {
    if (w_ <= 0) return;
    const View v{world_w / w_, world_w * 0.5f, world_h * 0.5f};
    if (const std::optional<Circle> a = source_.button(0)) {
        const bool held = source_.owner(Control::A).has_value();
        sprite(batch, star, v, a->center, fl(a->radius) * 1.8f, held ? 1.9f : 0.7f, held ? 0.85f : 0.5f);
    }
    const std::optional<Circle> base = source_.stick_base();
    const std::optional<Vec2> knob = source_.stick_knob();
    if (base && knob) {
        sprite(batch, star, v, base->center, fl(base->radius) * 2.0f, 0.5f, 0.3f);
        sprite(batch, star, v, *knob, fl(base->radius) * 0.9f, 1.4f, 0.7f);
    }
}

} // namespace game
