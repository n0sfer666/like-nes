#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>

#include "gpu.hpp"

// Кадр оконной петли из поверхности — одно место на все окна рендера, образца, shell'ов и мобильных
// оболочек (аудит #21 A·3·6). Копии ветки отказа разошлись: одна крутила ядро на мёртвой
// поверхности и не выходила вовсе, потому что не переконфигурировала её и не считала пропуск.
enum class FrameAction { Draw, Reconfigure, Skip, Quit };

// Чистая: что делать с кадром при этом статусе. Потерянное устройство (A·3·11) — всегда выход:
// переконфигурация на нём возвращает тот же Outdated, и петля крутилась бы без диагностики.
FrameAction frame_action(WGPUSurfaceGetCurrentTextureStatus status, bool device_lost);

// Поверхность и то, чем её конфигурировали. Формат выбран ОДИН раз: под него собраны пайплайны, и
// переконфигурация после смены монитора другим `formats[0]` ломала бы каждый кадр валидацией.
struct SurfaceSpec {
    WGPUSurface surface = nullptr;
    WGPUTextureFormat format = WGPUTextureFormat_Undefined;
    uint32_t width = 0;
    uint32_t height = 0;
};

// Первая конфигурация: формат — первый из поддерживаемых. Поверхность на `Fifo`: показ служит
// тактом петли.
WGPUTextureFormat configure_surface(WGPUSurface s, WGPUAdapter a, WGPUDevice d,
                                    uint32_t w, uint32_t h);
// Повторная — ресайз или устаревшая поверхность — тем же форматом.
void reconfigure_surface(const SurfaceSpec& spec, WGPUDevice d);

struct SurfaceFrame {
    WGPUTexture texture = nullptr;
    bool quit = false;
};

// Текстура кадра или её отсутствие. Без текстуры кадр пропущен, но петля ОБЯЗАНА считать его к
// автовыходу; `quit` — выйти ненулевым кодом, причина уже напечатана с префиксом `tag`. Пропуск
// печатается один раз на сессию (`warned`), выход — всегда.
SurfaceFrame acquire_frame(const SurfaceSpec& spec, const GpuContext& gpu, const char* tag,
                           bool& warned);
