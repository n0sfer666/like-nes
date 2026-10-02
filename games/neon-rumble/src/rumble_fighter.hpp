#pragma once
#include <cstdint>

#include "bundle_lookup.hpp"
#include "clip_read.hpp"
#include "fixmath.hpp"
#include "layer_quads.hpp"

namespace rumble {

struct Level;

// Поза бойца на тике: витрина перебирает клипы по кругу, каждый второй круг — лицом в другую
// сторону, чтобы флип был виден без управления (оно приходит с #25).
struct Pose {
    framework::graphics::ClipView clip;
    const char* name = "";
    uint16_t frame = 0;
    bool flip = false;
};

// Боец из `game.bundle` (спека #24, В6б): клипы `clips`, лист их текстуры сырым RGBA8 и точка
// спавна класса `spawn` из таблицы `objects` уровня. Лист смотрит в маппинг файла уровня.
struct Fighter {
    framework::graphics::ClipTable clips;
    asset::RgbaView sheet;
    framework::graphics::TextureSize sheet_size;
    framework::Vec2 spawn{};
    bool faces_left = false;

    bool open(const Level& level);
    Pose pose(uint64_t tick) const;
};

} // namespace rumble
