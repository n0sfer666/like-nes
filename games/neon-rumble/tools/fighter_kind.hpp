#pragma once

#include <span>

#include "assetc_kind.hpp"

namespace rumble {

// Виды записей манифеста Neon Rumble (спека #29, В2а): `fighter|<имя>|<путь>.fighter`. Бойца
// печёт инструмент игры, а не ядро пекаря SDK — жанровой записи в SDK нет.
std::span<const asset::manifest::Kind> assetc_kinds();

} // namespace rumble
