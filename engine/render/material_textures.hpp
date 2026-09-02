#pragma once

#include <webgpu/webgpu.h>

// Картинки сцены гейтов считаются в коде, а не читаются с диска: движковому гейту нужен кадр,
// воспроизводимый на любой машине, а файл привязал бы его к бандлу игры-образца.
namespace matgold {

// Спрайт: непрозрачный круг в квадрате 32×32, за кругом альфа 0 — обводке и растворению нужна
// кромка, а сплошной квад её не даёт.
WGPUTexture make_sprite_texture(WGPUDevice device, WGPUQueue queue);

// Шум для растворения. Детерминированный: тот же байт на любой машине, иначе кадр гейта расходится
// не из-за бэкенда, а из-за генератора.
WGPUTexture make_noise_texture(WGPUDevice device, WGPUQueue queue);

} // namespace matgold
