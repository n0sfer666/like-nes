#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <vector>

class Renderer;
struct SceneSnapshot;

// Offscreen readback + golden-сравнение (T4). Readback идёт ТОЛЬКО в файл/тест —
// НИКОГДА обратно в симуляцию (инвариант «GPU не кормит сим»).
namespace capture {

// Рендер одного кадра в offscreen RGBA8-таргет, чтение пикселей обратно на CPU.
std::vector<uint8_t> render_offscreen(WGPUDevice device, WGPUQueue queue, Renderer& renderer,
                                      const SceneSnapshot& snap, uint32_t w, uint32_t h);

bool write_png(const char* path, const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h);
bool read_png(const char* path, std::vector<uint8_t>& out, uint32_t& w, uint32_t& h);

// Perceptual/epsilon-сравнение (НЕ побайтовый хеш): средняя/макс. ошибка канала [0..1]
// и доля пикселей за порогом. GPU-рендер не бит-в-бит между вендорами/драйверами.
struct DiffResult {
    double mean_abs;
    double max_abs;
    double frac_over;
    bool pass;
};
DiffResult compare(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                   double per_pixel_eps, double frac_tolerance);

} // namespace capture
