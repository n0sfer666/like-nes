#pragma once

#include <webgpu/webgpu.h>

#include <string>

namespace mat {

// Устройство ПОД ВАЛИДАЦИЮ, по одному на целевой бэкенд.
//
// Не `GpuContext` из render по двум причинам, и обе несущие. Направление зависимости: рисующий
// знает про материалы, материалы про рисующего не знают (инвариант 1 спеки #14), а валидатор
// живёт здесь, потому что им пользуется ПЕКАРЬ, который не рисует вовсе. И назначение: у
// контекста отрисовки отказ адаптера — конец работы, здесь же «этого бэкенда на машине нет» —
// штатный исход, который обязан быть НАЗВАН и пропущен, а не превращён в отказ бейка.
struct ValidationDevice {
    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;

    // false — бэкенда на этой машине нет; `skip_reason` говорит, на каком шаге он кончился.
    bool open(WGPUBackendType backend, std::string& skip_reason);
    void close();
};

// Имя бэкенда для отчёта. Строкой, а не числом: отчёт читает человек и грепает гейт.
const char* backend_name(WGPUBackendType backend);

} // namespace mat
