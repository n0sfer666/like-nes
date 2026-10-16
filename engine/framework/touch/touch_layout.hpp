#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "fixed.hpp"

// Раскладка экранного тача — данные игры, а не код: `touch | <имя> | layout | <путь>.txt` в
// манифесте, секция `LNTL` в бандле. Все длины — в точках экрана (iOS points, Android px / density),
// а не в пикселях: в пикселях кнопка была бы разной на экранах разной плотности. Числа — fix32,
// потому что из них считается ось стика, а она уходит в InputFrame и в sim-хеш.
namespace framework::touch {

constexpr uint32_t BUTTONS = 5;   // touch:a … touch:e, коды `input::code::Touch`

// Кнопка привязана к углу safe area, а не экрана: на телефоне с вырезом угол экрана лежит под
// вырезом или под полосой жестов, и кнопка там не нажимается.
enum class Anchor : uint32_t { BottomLeft = 0, BottomRight = 1, TopLeft = 2, TopRight = 3 };

struct ButtonSpec {
    uint32_t button = 0;
    Anchor anchor = Anchor::BottomRight;
    fix32 dx, dy;                 // от угла safe area внутрь до центра кнопки
    fix32 radius;
};

struct Layout {
    fix32 stick_radius;           // длина поводка плавающего стика: полный наклон на этом расстоянии
    std::array<ButtonSpec, BUTTONS> buttons{};
    uint32_t button_count = 0;
};

// Потолки — из размеров экрана, а не из формата: кнопка радиусом в тысячу точек или в десяти
// тысячах от угла лежит за любым экраном, и такой бандл — порча, а не раскладка.
constexpr int32_t MAX_RADIUS_PT = 256;
constexpr int32_t MAX_OFFSET_PT = 2048;

bool parse_layout(std::string_view text, const std::string& file, Layout& out, std::string& error);
bool bake_layout(const Layout& layout, std::vector<uint8_t>& out, std::string& error);
// Читатель отбивает всё, что отбил бы пекарь: правило одно, и бандл, собранный чужим пекарем, не
// проносит раскладку, которую свой отказался бы печь.
bool read_layout(const void* data, std::size_t size, Layout& out);
bool layout_ok(const Layout& layout, std::string& error);

} // namespace framework::touch
