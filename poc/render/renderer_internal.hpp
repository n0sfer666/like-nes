#pragma once
#include <webgpu/webgpu.h>

// Разделяемое между renderer.cpp (init/render) и renderer_pipelines.cpp (сборка пайплайнов).
struct SpriteUniform {
    float pos[2];
    float scale;
    float rot;
    float aspect;
    float alpha;
    float emissive;
    float _pad;
};

inline WGPUColorTargetState color_target(WGPUTextureFormat fmt) {
    WGPUColorTargetState t = {};
    t.format = fmt;
    t.blend = nullptr;
    t.writeMask = WGPUColorWriteMask_All;
    return t;
}
