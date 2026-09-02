#pragma once

#include <webgpu/webgpu.h>

#include <string>

namespace mat::detail {

// Область ошибки вокруг ОДНОГО вызова создания.
//
// Судить об успехе по возвращённому указателю нельзя, и это измерено: на битом WGSL и
// `wgpuDeviceCreateShaderModule`, и `wgpuDeviceCreateRenderPipeline` отдают НЕНУЛЕВОЙ объект, а
// ошибку рассказывают только области или глобальному коллбэку. Пока проверялся указатель, битый
// модуль считался собранным, и отказ всплывал позже — уже без имени шейдера.
void error_scope_begin(WGPUDevice device);

// Пусто — ошибки не было. Иначе полный текст валидатора (разбирает его `parse_wgpu_error`).
// Область, не ответившая ни разу, отдаёт текст об этом, а не пустоту: «не спросили» и «ошибок
// нет» обязаны различаться, иначе валидатор зелен вакуумно.
std::string error_scope_end(WGPUDevice device);

} // namespace mat::detail
