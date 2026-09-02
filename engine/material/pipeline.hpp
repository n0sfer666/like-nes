#pragma once

#include <webgpu/webgpu.h>

#include <cstdint>

namespace mat::detail {

// Сборка пайплайна вынесена из кэша: у кэша вопрос «какой пайплайн у этого материала», здесь —
// «как он устроен». Раскладка вершинных атрибутов и состояния смешивания больше нигде не нужны, а
// вместе с поиском по ключу они дают файл, в котором две ответственности.
WGPUShaderModule make_module(WGPUDevice device, const char* wgsl);

// Точка входа ищется в ТЕКСТЕ модуля до создания пайплайна. Не оптимизация: материал, назвавший
// несуществующий вход, иначе доходит до валидатора, и тот отвечает ошибкой устройства, в которой
// имени материала нет — а есть оно только здесь.
bool has_entry(const char* wgsl, const char* entry);

// Пайплайн библиотеки: quad-VB + инстансный буфер, один цветовой таргет, смешивание по `blend`.
// Возвращает nullptr, если валидатор отказал, — решение о запасном принимает кэш.
WGPURenderPipeline make_pipeline(WGPUDevice device, WGPUPipelineLayout layout,
                                 WGPUShaderModule module, const char* entry, uint8_t blend,
                                 WGPUTextureFormat target);

// Запасной модуль: рисует ровный маркерный цвет и собирается всегда. ASCII-строка в коде, а не
// файл на диске: запасной путь, которому нужен читаемый файл, отказывает ровно тогда, когда он
// единственный, кто ещё работает.
const char* fallback_wgsl();

} // namespace mat::detail
