#pragma once
#include "../src/fixed.hpp"
#include <flecs.h>
#include <cstdint>
#include <string>

// Компоненты сцены редактора (спека #7, срез 1). fix32 — детерм. значения (opaque meta-тип →
// raw int в JSON, целочисл. golden cross-arch). Иерархия = opt-in `Parent`-компонент (guid-ссылка,
// не навязана). Одна meta-регистрация обслуживает property-grid + IPC-зеркало + save/load.
namespace ide {

struct Name { std::string value; };
struct Parent { uint64_t guid; };
struct Position { fix32 x, y; };
struct Velocity { fix32 x, y; };

// Регистрирует opaque-типы (fix32, std::string) и компоненты с meta-членами. Идемпотентна на мир.
void register_types(flecs::world& w);

} // namespace ide
