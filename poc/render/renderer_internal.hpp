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

// std140-совместимый layout светов (совпадает с WGSL LightsU): vec4-выровнен.
struct LightsUniform {
    float header[4];       // count, aspect, ambient, _
    float ambient_col[4];  // rgb ambient
    float lights[6][4];    // на свет: [x,y,z,radius], [r,g,b,intensity]
};

inline WGPUColorTargetState color_target(WGPUTextureFormat fmt) {
    WGPUColorTargetState t = {};
    t.format = fmt;
    t.blend = nullptr;
    t.writeMask = WGPUColorWriteMask_All;
    return t;
}
