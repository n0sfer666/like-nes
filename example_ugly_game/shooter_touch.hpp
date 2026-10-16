#pragma once
#include <cstdint>
#include <string>

#include "art.hpp"
#include "batch.hpp"
#include "input_engine.hpp"
#include "mobile_app.hpp"
#include "touch_source.hpp"

namespace game {

// Тач шутера: плавающий стик и кнопка a из ассета `touch` бандла, касания уходят в движок ввода
// устройством Touch, а пресет вяжет их с `fire` и `move_*` так же, как пад. Оболочка отдаёт
// float-точки, здесь они становятся fix32: ось стика из них попадает в sim-хеш.
class ShooterTouch {
public:
    bool load(const std::string& bundle_path);
    void bind(input::InputEngine* engine);
    void set_view(float w_pt, float h_pt, const mobile::SafeArea& safe);
    void pointer(intptr_t id, mobile::Touch phase, float x_pt, float y_pt);
    void cancel() { source_.cancel(); }
    // Демо-пад, пока экран свободен; палец на экране отпускает пад до своего Up.
    void demo(uint32_t tick);
    // Чьё касание держит стик и кнопку a (-1 — ничьё): гейт симулятора судит маршрутизацию по ним.
    std::string state() const;
    void push(SpriteBatch& batch, const Region& star, float world_w, float world_h) const;

private:
    void post_pad(input::RawKind kind, uint16_t code, int32_t value);
    void yield_pad();

    // Отдельный диапазон seq от тача, как у источников пада на десктопе.
    static constexpr uint64_t PAD_SEQ_BASE = 1'000'000;

    framework::touch::TouchSource source_;
    input::InputEngine* engine_ = nullptr;
    uint64_t pad_seq_ = PAD_SEQ_BASE;
    bool pad_connected_ = false, pad_moving_ = false, pad_a_ = false;
    float w_ = 0;
};

} // namespace game
