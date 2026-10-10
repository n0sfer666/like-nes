#pragma once
#include <cstdint>
#include <string>

#include "action_map.hpp"
#include "presets.hpp"
#include "rebind_store.hpp"

// Раскладка игрока собирается целиком — пресет, поверх него ЕГО накладка — и только потом ставится
// в карту. Пошаговая заливка в живую карту оставляла бы игрока с половиной раскладки на первом же
// отказе, а отказ здесь штатный: накладка P2, забравшая клавишу P1, обязана не встать, а быть
// названа игроку текстом.
namespace framework::input {

bool build_layout(const PresetTable& table, uint32_t preset, const RebindStore& store,
                  ::input::ActionMap& map, int player, std::string& error);

// Отказ ActionMap человеческими словами: игроки и пады с единицы, вход — тем же именем, что в манифесте.
std::string shared_input_text(const ::input::SharedInput& shared);

} // namespace framework::input
