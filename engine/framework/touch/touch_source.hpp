#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "fixmath.hpp"
#include "input_engine.hpp"
#include "touch_layout.hpp"

// Экранный тач как устройство ввода: касания оболочки → `RawKind::TouchButton*`/`TouchAxis` в
// движок ввода. Симуляция видит только InputFrame и экран от пада не отличает; раскладка и
// мёртвая зона — у пресета, как у пада.
//
// Геометрия — в fix32-точках: из неё считается ось стика, а ось уходит в sim-хеш. Оболочка переводит
// свои float-координаты в fix32 до вызова `pointer`, поэтому здесь float нет.
namespace framework::touch {

enum class Phase : uint8_t { Down, Move, Up };
enum class Control : uint8_t { A = 0, B, C, D, E, Stick };

struct Insets {
    fix32 left, top, right, bottom;
    bool operator==(const Insets&) const = default;
};
struct Circle { Vec2 center; fix32 radius; };

class TouchSource {
public:
    void bind(::input::InputEngine* engine);
    // Неверная раскладка не принимается: номер кнопки за `BUTTONS` индексировал бы удержания мимо
    // массива, а нулевой радиус стика давал бы ось 0/0. Источник остаётся без раскладки и без стика.
    bool set_layout(const Layout& layout);
    // Размер вида и safe area — в точках, origin слева сверху, y вниз. Смена вида (поворот, другой
    // вырез) отпускает касания: центр стика и кнопки остались бы в старых координатах.
    void set_view(fix32 width, fix32 height, const Insets& safe);
    void pointer(intptr_t id, Phase phase, fix32 x, fix32 y);
    // Система отняла касания (звонок, жест, уход в фон): всё удержанное отпускается в том же тике.
    void cancel();

    // Чьё касание держит элемент: шов пробы гейтов, в игре не нужен.
    std::optional<intptr_t> owner(Control c) const;
    bool idle() const;
    // Где нарисовать: кнопка раскладки (нет в раскладке — nullopt) и стик, пока его держат.
    std::optional<Circle> button(uint32_t button) const;
    std::optional<Circle> stick_base() const;
    std::optional<Vec2> stick_knob() const;

private:
    struct Hold { intptr_t id = 0; bool held = false; };

    void post(::input::RawKind kind, uint16_t code, int32_t value);
    void post_stick(Vec2 v);
    void release(uint32_t control);
    void release_id(intptr_t id);
    bool hit(uint32_t button, Vec2 p) const;
    void drag(Vec2 p);

    ::input::InputEngine* engine_ = nullptr;
    uint64_t seq_ = 0;
    Layout layout_;
    fix32 width_, height_;
    Insets safe_;
    std::array<Hold, BUTTONS + 1> holds_{};
    Vec2 center_, finger_;
};

} // namespace framework::touch
