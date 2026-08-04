#pragma once
#include <cstdint>
#include <vector>

#include "action_map.hpp"
#include "presets.hpp"
#include "rebind_store.hpp"

// Раскладка игры приезжает из бандла (спека #14): пресет — ассет, а не код. Держим байты у себя,
// потому что таблица только смотрит в чужую память, а бандл закрывается сразу после чтения.
namespace game {

struct Controls {
    std::vector<uint8_t> blob;
    framework::input::PresetTable table;
    framework::input::RebindStore rebinds;
    input::ActionMap map;
};

// Бандл → пресет `default` → накладка игрока из каталога сейвов. false = раскладки нет: играть
// без управления нечем, и молчаливый пустой ActionMap выглядел бы как «игра не реагирует».
bool load_controls(Controls& out);

} // namespace game
