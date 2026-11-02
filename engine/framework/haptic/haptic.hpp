#pragma once
#include <cstdint>

namespace haptic {

enum class Kind { Phone, Pad };
struct Target { Kind kind; int pad_slot = -1; };

// Вызывать на потоке игры. Оболочка владеет init/shutdown и паузой; игра — только pulse.
// strength0..255; strength0 или ms0 отменяют импульс цели.
// До init и в suspend запросы игнорируются. Phone без мотора — безопасный no-op.
void init();
void pulse(Target target, uint8_t strength, uint16_t ms);
void suspend();
void resume();
void shutdown();

} // namespace haptic
