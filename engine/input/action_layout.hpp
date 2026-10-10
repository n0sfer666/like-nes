#pragma once
#include <vector>
#include "input_types.hpp"

// Раскладка ОДНОГО игрока — значение: собирается целиком и только потом ставится в ActionMap,
// который отказывает, если она делит физический вход с другим игроком.
namespace input {

struct Source {
    SourceKind kind = SourceKind::None;
    uint16_t code = 0;  // key/button/axis-код
    int8_t sign = 1;    // направление для оси-из-клавиш / инверсия pad-оси
};

struct ActionBinding {
    int action = 0;
    Source src;
    int context = 0;
};

// Ось: пара источников (pos/neg — клавиши-как-ось) ИЛИ одиночная pad/mouse-ось.
struct AxisBinding {
    int axis = 0;
    Source pos;         // + направление (или сама ось для pad/mouse)
    Source neg;         // - направление (kind==None если одиночная ось)
    fix32 deadzone;     // радиус мёртвой зоны (fix32)
    int context = 0;
    fix32 scale = fix32::from_int(1); // чувствительность (напр. масштаб дельты мыши)
};

class ActionLayout {
public:
    void bind(int action, Source src, int context = 0) { buttons_.push_back({action, src, context}); }
    void bind_axis(int axis, Source pos, Source neg, fix32 dz, int context = 0, fix32 scale = fix32::from_int(1)) {
        axes_.push_back({axis, pos, neg, dz, context, scale});
    }

    // Rebind (карта = данные): заменить which-й биндинг действия / очистить действие.
    void rebind(int action, int which, Source s) {
        int seen = 0;
        for (ActionBinding& b : buttons_)
            if (b.action == action && seen++ == which) { b.src = s; return; }
    }
    void clear_action(int action) {
        for (auto it = buttons_.begin(); it != buttons_.end();)
            it->action == action ? it = buttons_.erase(it) : ++it;
    }

    const std::vector<ActionBinding>& buttons() const { return buttons_; }
    const std::vector<AxisBinding>& axes() const { return axes_; }

private:
    std::vector<ActionBinding> buttons_;
    std::vector<AxisBinding> axes_;
};

} // namespace input
