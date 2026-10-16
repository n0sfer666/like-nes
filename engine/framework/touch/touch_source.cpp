#include "touch_source.hpp"

#include <string>

#include "codes.hpp"

namespace framework::touch {
namespace {

constexpr uint32_t STICK = static_cast<uint32_t>(Control::Stick);

bool left(Anchor a) { return a == Anchor::BottomLeft || a == Anchor::TopLeft; }
bool top(Anchor a) { return a == Anchor::TopLeft || a == Anchor::TopRight; }

} // namespace

void TouchSource::bind(::input::InputEngine* engine) {
    engine_ = engine;
    seq_ = 0;
}

bool TouchSource::set_layout(const Layout& layout) {
    cancel();
    std::string why;
    const bool ok = layout_ok(layout, why);
    layout_ = ok ? layout : Layout{};
    return ok;
}

void TouchSource::set_view(fix32 width, fix32 height, const Insets& safe) {
    if (!(width == width_ && height == height_ && safe == safe_)) cancel();
    width_ = width;
    height_ = height;
    safe_ = safe;
}

void TouchSource::post(::input::RawKind kind, uint16_t code, int32_t value) {
    if (engine_ == nullptr) return;
    engine_->post({kind, ::input::DeviceKind::Touch, 0, code, value, seq_++});
}

void TouchSource::post_stick(Vec2 v) {
    post(::input::RawKind::TouchAxis, static_cast<uint16_t>(::input::code::StickX), v.x.raw);
    post(::input::RawKind::TouchAxis, static_cast<uint16_t>(::input::code::StickY), v.y.raw);
}

std::optional<Circle> TouchSource::button(uint32_t button) const {
    for (uint32_t i = 0; i < layout_.button_count; ++i) {
        const ButtonSpec& b = layout_.buttons[i];
        if (b.button != button) continue;
        const fix32 x = left(b.anchor) ? safe_.left + b.dx : width_ - safe_.right - b.dx;
        const fix32 y = top(b.anchor) ? safe_.top + b.dy : height_ - safe_.bottom - b.dy;
        return Circle{{x, y}, b.radius};
    }
    return std::nullopt;
}

// Модуль, а не квадрат: квадрат расстояния в точках насыщается на 32767 (≈181 pt), и касание у
// другого края экрана попадало бы в кнопку.
bool TouchSource::hit(uint32_t button, Vec2 p) const {
    const std::optional<Circle> c = this->button(button);
    return c && !(c->radius < length(p - c->center));
}

void TouchSource::pointer(intptr_t id, Phase phase, fix32 x, fix32 y) {
    const Vec2 p{x, y};
    if (phase == Phase::Up) {
        release_id(id);
        return;
    }
    const Hold& stick = holds_[STICK];
    if (phase == Phase::Move) {
        if (stick.held && stick.id == id) drag(p);
        return;
    }
    release_id(id);
    for (uint32_t i = 0; i < layout_.button_count; ++i) {
        const uint32_t b = layout_.buttons[i].button;
        if (!hit(b, p)) continue;
        if (holds_[b].held) return;
        holds_[b] = {id, true};
        post(::input::RawKind::TouchButtonDown, static_cast<uint16_t>(b), 0);
        return;
    }
    if (stick.held || !(fix32{} < layout_.stick_radius) || !(x < mid_fix(fix32{}, width_))) return;
    holds_[STICK] = {id, true};
    center_ = finger_ = p;
}

// Плавающий стик: центр встаёт под первое касание и тянется за пальцем, ушедшим дальше радиуса, —
// обратный ход сразу даёт наклон в другую сторону, без мёртвого пути до старого центра.
void TouchSource::drag(Vec2 p) {
    const fix32 r = layout_.stick_radius;
    if (r < length(p - center_)) center_ = p - clamp_length(p - center_, r);
    finger_ = p;
    const Vec2 d = p - center_;
    post_stick(clamp_length({d.x / r, d.y / r}, fix32::from_int(1)));
}

void TouchSource::release(uint32_t control) {
    holds_[control].held = false;
    if (control == STICK) post_stick({});
    else post(::input::RawKind::TouchButtonUp, static_cast<uint16_t>(control), 0);
}

// Down от id, который ещё что-то держит, значит, его Up потерян (Android переиспользует id, первый
// палец почти всегда 0): без отпускания залипли бы и элемент, и все следующие касания этим id.
void TouchSource::release_id(intptr_t id) {
    for (uint32_t c = 0; c < holds_.size(); ++c)
        if (holds_[c].held && holds_[c].id == id) release(c);
}

void TouchSource::cancel() {
    for (uint32_t c = 0; c < holds_.size(); ++c)
        if (holds_[c].held) release(c);
}

std::optional<intptr_t> TouchSource::owner(Control c) const {
    const Hold& h = holds_[static_cast<uint32_t>(c)];
    return h.held ? std::optional<intptr_t>(h.id) : std::nullopt;
}

bool TouchSource::idle() const {
    for (const Hold& h : holds_)
        if (h.held) return false;
    return true;
}

std::optional<Circle> TouchSource::stick_base() const {
    if (!holds_[STICK].held) return std::nullopt;
    return Circle{center_, layout_.stick_radius};
}

std::optional<Vec2> TouchSource::stick_knob() const {
    if (!holds_[STICK].held) return std::nullopt;
    return finger_;
}

} // namespace framework::touch
