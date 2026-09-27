#pragma once

namespace game {

// У NativeActivity stdout и stderr ведут в /dev/null: стартовые строки игры ([gpu], [game] materials,
// audio, achievements) — ровно то, чем владелец проверяет подсистемы, — на устройстве пропадали.
// Здесь оба потока заворачиваются в канал, а поток-читатель пишет его построчно в logcat под тегом
// like-nes. false — канал не завёлся, вывод остаётся там, где был.
bool stdout_to_logcat();

} // namespace game
