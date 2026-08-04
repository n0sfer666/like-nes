#pragma once
#include <webgpu/webgpu.h>

struct GLFWwindow;
struct GpuContext;

// Гейт 6 спеки #13 в исполняемом виде: редактор сам проходит сценарий, ради которого владельца
// просили открыть его руками на X11- и на Wayland-сессии, и оставляет доказательства — паспорт
// сессии в stdout и кадр в PNG. Ручным остаётся то, чего процесс о себе знать не может: видно ли
// окно на экране и слушается ли гизмо настоящей мыши.
namespace ide::editor {

struct EditorState;

// Возврат: 0 — все проверки прошли; 1 — есть провалившиеся (перечислены в stdout).
// out_png = nullptr → кадр не снимается (проверки данных и рендер-цикл всё равно идут).
int run_gate6(EditorState& st, GLFWwindow* win, const GpuContext& gpu, WGPUSurface surface,
              WGPUTextureFormat fmt, const char* out_png);

} // namespace ide::editor
