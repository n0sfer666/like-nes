#pragma once
#include "scene.hpp"
#include <cstdint>
#include <string>

// Детерминированная текст-сериализация сцены (гейт 1). Формат — строковый конверт вокруг
// meta-driven flecs-JSON значений компонентов:
//   # like-nes scene v1
//   E <guid>
//   C <ComponentName> <flecs-json-одна-строка>
// Сущности по GUID (std::map), компоненты в фикс. алфавитном порядке → байт-детерминизм.
// Значения — через flecs meta (to_json/from_json), НЕ per-component hand-code (одна рефлексия).
namespace ide {

std::string serialize(const Scene& s);
void deserialize(Scene& s, const std::string& text);

// Golden bake-hash: FNV-1a над канонич. текстом. Текст = ASCII + целочисл. fix32-raw →
// run-to-run + cross-machine стабилен (тот же паттерн, что asset/audio/input/plugin golden).
uint64_t golden_hash(const Scene& s);

// Per-entity snapshot (только `C ...`-строки, без `E`). Для undo destroy_entity — восстановить
// сущность с теми же компонентами. Переиспользует ту же meta-рефлексию.
std::string serialize_entity(const Scene& s, uint64_t guid);
void restore_entity(Scene& s, uint64_t guid, const std::string& body);

} // namespace ide
