#include "shaders.hpp"

const char* tonemap_fs_wgsl() {
    return R"WGSL(
@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var hdrTex: texture_2d<f32>;

@fragment
fn fs(@location(0) uv: vec2f) -> @location(0) vec4f {
    let c = textureSample(hdrTex, samp, uv).rgb;
    let mapped = c / (c + vec3f(1.0));
    return vec4f(mapped, 1.0);
}
)WGSL";
}
