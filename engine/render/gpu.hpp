#pragma once
#include <webgpu/webgpu.h>

#include <atomic>

// Общий WebGPU-контекст для оконного (demo) и headless (golden) путей.
// surface может быть null — тогда адаптер запрашивается без compatibleSurface (offscreen/CI).
struct GpuContext {
    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    bool supports_bc = false; // device включил TextureCompressionBC (baked BC7-шов доступен)
    // Устройство потеряно (TDR, отключённая eGPU, переключение гибридной графики): причина уже
    // напечатана строкой `[gpu] device lost`, оконная петля выходит ненулевым кодом (аудит #21
    // A·3·11). Без своего коллбэка wgpu-native падал бы panic!'ом. Коллбэк держит адрес флага, а
    // поток его вызова webgpu.h не фиксирует: atomic заодно запрещает копировать и перемещать
    // контекст, у копии флаг никто бы не взвёл.
    std::atomic<bool> device_lost{false};

    // Чем выбирать адаптер. Заполняется вызывающим ДО init; по умолчанию — «решает wgpu» и
    // дискретная карта, то есть ровно прежнее поведение. Полями, а не чтением окружения внутри:
    // gpu.cpp компилируется НАПРЯМУЮ в шесть целей, включая iOS и Android, у которых нет
    // platform_core, а std::getenv в дереве запрещён. Настройки, доступные в окружении, ставит
    // тот, у кого шов уже есть (example_ugly_game/gpu_env.cpp).
    //
    // Ручки нужны не для тюнинга, а для диагностики: на гибридном ноутбуке (Intel iGPU +
    // дискретная NVIDIA) кадр может рисоваться на одной карте, а окном владеть другая — изнутри
    // процесса всё успешно, ни ошибки устройства, ни отказа поверхности, а экран чёрный.
    // Отличить такое от поломки движка можно только сменив бэкенд или адаптер.
    WGPUBackendType backend = WGPUBackendType_Undefined;
    WGPUPowerPreference power = WGPUPowerPreference_HighPerformance;

    // Инстанс с учётом backend. Кто заполнил backend и создаёт инстанс сам (оконный путь: surface
    // нужен раньше init), обязан звать ЕГО, а не wgpuCreateInstance: отбор бэкендов wgpu-native
    // делает на инстансе (WGPUInstanceExtras), а WGPURequestAdapterOptions.backendType игнорирует.
    // Кто backend не трогает, ничего не теряет — при Undefined это тот же wgpuCreateInstance(null).
    WGPUInstance create_instance() const;

    bool init(WGPUSurface surface);
    void shutdown();
};
