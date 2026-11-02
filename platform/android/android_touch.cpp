#include "android_touch.hpp"

#include <cstdio>

#include "mobile_probe.hpp"

namespace {

void dispatch(mobile::Game& game, AInputEvent* ev, size_t idx, mobile::Touch phase, float scale,
              const std::string& probe) {
    const int32_t id = AMotionEvent_getPointerId(ev, idx);
    const float x = AMotionEvent_getX(ev, idx) / scale, y = AMotionEvent_getY(ev, idx) / scale;
    game.pointer(id, phase, x, y);
    if (!probe.empty()) std::fprintf(stderr, "%s\n", mobile::touch_line(id, phase, x, y, game.touch_state(),
                                                                        probe.c_str()).c_str());
}

} // namespace

// Касания приходят в пикселях окна, игра ждёт точки: 160dpi — одна точка на пиксель.
float android_touch_scale(AConfiguration* config) {
    const int32_t d = config ? AConfiguration_getDensity(config) : 0;
    if (d <= 0 || d == ACONFIGURATION_DENSITY_NONE || d == ACONFIGURATION_DENSITY_ANY) return 1;
    return static_cast<float>(d) / static_cast<float>(ACONFIGURATION_DENSITY_MEDIUM);
}

int32_t android_touch(mobile::Game& game, AInputEvent* ev, float scale, const std::string& probe) {
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION || scale <= 0) return 0;
    // Джойстики тоже посылают MotionEvent: сенсорный экран не должен забирать их касаниями.
    if ((AInputEvent_getSource(ev) & AINPUT_SOURCE_TOUCHSCREEN) != AINPUT_SOURCE_TOUCHSCREEN) return 0;
    const int32_t raw = AMotionEvent_getAction(ev);
    const int32_t action = raw & AMOTION_EVENT_ACTION_MASK;
    const size_t count = AMotionEvent_getPointerCount(ev);
    const size_t idx = static_cast<size_t>((raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                                           >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            if (idx < count) dispatch(game, ev, idx, mobile::Touch::Down, scale, probe);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            for (size_t i = 0; i < count; ++i) dispatch(game, ev, i, mobile::Touch::Move, scale, probe);
            break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            if (idx < count) dispatch(game, ev, idx, mobile::Touch::Up, scale, probe);
            break;
        case AMOTION_EVENT_ACTION_CANCEL:
            game.cancel();
            break;
        default: return 0;
    }
    return 1;
}
