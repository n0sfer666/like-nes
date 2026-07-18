#pragma once
#include <string>

// WGSL-исходники напрямую (спека #2: «WGSL + material-система сверху»).
// Общий lighting-модуль (шаги 2-3) текстово включается И в deferred, И в forward —
// это доказывает шов абстракции lighting-technique (второй потребитель).

const char* gbuffer_wgsl();
const char* fullscreen_vs_wgsl();
const char* preview_fs_wgsl();

std::string lighting_module_wgsl();
std::string deferred_lighting_wgsl();
std::string forward_wgsl();

const char* brightpass_fs_wgsl();
const char* blur_fs_wgsl();
const char* tonemap_fs_wgsl();
