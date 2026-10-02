#pragma once

namespace asset {

// `assetc --manifest <file> <out.bundle> [--depfile D] [--basisu P]` (спека #24, В4) — режим, через
// который `like_nes_bake` печёт бандл игры. Код выхода: 0 — бандл записан, 1 — бейк отказал,
// 2 — неверный вызов.
int run_manifest(int argc, char** argv);

} // namespace asset
