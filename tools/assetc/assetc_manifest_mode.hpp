#pragma once
#include <span>

#include "assetc_kind.hpp"

// Точка входа режима манифеста для любого пекаря (спека #29, В2а): базовый `assetc` зовёт её без
// видов, инструмент игры — со своей таблицей.
namespace asset::manifest {

// `<инструмент> --manifest <file> <out.bundle> [--depfile D] [--basisu P]`. Код выхода: 0 — бандл
// записан, 1 — бейк отказал, 2 — неверный вызов.
int run_manifest(int argc, char** argv, std::span<const Kind> kinds);

} // namespace asset::manifest
