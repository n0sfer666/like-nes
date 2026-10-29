#pragma once

#include <string>
#include <vector>

#include "bundle_writer.hpp"

namespace game {

// Профиль движения (спека #16): тот же zero-parse шов, что у пресетов. Настройка ОЩУЩЕНИЯ правится
// десятками итераций подряд, и пересборка движка на каждую правку высоты прыжка убивает сам цикл
// подбора — поэтому профиль едет данными; guid = fnv1a("movement"). Секция жанровая и живёт у игры
// (спека #29, В2а), а не в ядре пекаря.
bool movement_section(const std::string& src, std::vector<asset::AssetInput>& out);

} // namespace game
