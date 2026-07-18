#include "shaders.hpp"

const char* gbuffer_wgsl() {
    return R"WGSL(
struct SpriteU {
    pos: vec2f,
    scale: f32,
    rot: f32,
    aspect: f32,
    alpha: f32,
    emissive: f32,
    _pad: f32,
};
@group(0) @binding(0) var<uniform> u: SpriteU;
@group(0) @binding(1) var samp: sampler;
@group(0) @binding(2) var albedoTex: texture_2d<f32>;
@group(0) @binding(3) var normalTex: texture_2d<f32>;

struct VSOut {
    @builtin(position) clip: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vs(@location(0) pos: vec2f, @location(1) uv: vec2f) -> VSOut {
    var p = pos * u.scale;
    let c = cos(u.rot);
    let s = sin(u.rot);
    p = vec2f(p.x * c - p.y * s, p.x * s + p.y * c);
    p += u.pos;
    p.x /= u.aspect;
    var o: VSOut;
    o.clip = vec4f(p, 0.0, 1.0);
    o.uv = uv;
    return o;
}

struct GBuffer {
    @location(0) albedo: vec4f,
    @location(1) normal: vec4f,
};

@fragment
fn fs(in: VSOut) -> GBuffer {
    let a = textureSample(albedoTex, samp, in.uv);
    if (a.a < 0.5) { discard; }
    let n = textureSample(normalTex, samp, in.uv).xyz * 2.0 - 1.0;
    var o: GBuffer;
    o.albedo = vec4f(a.rgb, u.emissive);
    o.normal = vec4f(normalize(n) * 0.5 + 0.5, 1.0);
    return o;
}
)WGSL";
}

const char* fullscreen_vs_wgsl() {
    return R"WGSL(
struct VSOut {
    @builtin(position) clip: vec4f,
    @location(0) uv: vec2f,
};
@vertex
fn vs(@builtin(vertex_index) vi: u32) -> VSOut {
    var p = array<vec2f, 3>(vec2f(-1.0, -3.0), vec2f(-1.0, 1.0), vec2f(3.0, 1.0));
    var o: VSOut;
    let xy = p[vi];
    o.clip = vec4f(xy, 0.0, 1.0);
    o.uv = vec2f((xy.x + 1.0) * 0.5, (1.0 - xy.y) * 0.5);
    return o;
}
)WGSL";
}

const char* preview_fs_wgsl() {
    return R"WGSL(
@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var srcTex: texture_2d<f32>;
@fragment
fn fs(@location(0) uv: vec2f) -> @location(0) vec4f {
    return textureSample(srcTex, samp, uv);
}
)WGSL";
}
