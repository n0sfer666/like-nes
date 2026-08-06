#include "shaders.hpp"

// Общий lighting-technique модуль: ЧИСТАЯ функция shade() + layout светов, без биндингов.
// Текстово включается И в deferred (fullscreen), И в forward (per-object) — второй
// потребитель доказывает шов абстракции lighting-technique (insert-later != rewrite).
std::string lighting_module_wgsl() {
    // ascii: allow комментарии внутри исходника шейдера
    return R"WGSL(
struct LightsU {
    header: vec4f,       // x=count, y=aspect, z=ambient
    ambient_col: vec4f,  // rgb — цвет ambient
    lights: array<vec4f, 6>, // на свет: [posx,posy,posz,radius], [r,g,b,intensity]
};

fn shade(albedo: vec3f, N: vec3f, worldPos: vec2f, L: LightsU) -> vec3f {
    var LL = L;
    var lit = LL.ambient_col.rgb * LL.header.z * albedo;
    let count = u32(LL.header.x);
    for (var i = 0u; i < count; i = i + 1u) {
        let p = LL.lights[i * 2u];
        let c = LL.lights[i * 2u + 1u];
        let toL = vec3f(p.xy - worldPos, p.z);
        let d = length(toL);
        let Ln = toL / max(d, 1e-4);
        let atten = pow(clamp(1.0 - d / p.w, 0.0, 1.0), 2.0);
        let ndl = max(dot(N, Ln), 0.0);
        lit = lit + albedo * c.rgb * (c.w * ndl * atten);
    }
    return lit;
}
)WGSL";
}

std::string deferred_lighting_wgsl() {
    return lighting_module_wgsl() + R"WGSL(
@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var albedoTex: texture_2d<f32>;
@group(0) @binding(2) var normalTex: texture_2d<f32>;
@group(0) @binding(3) var<uniform> lights: LightsU;

@fragment
fn fs(@location(0) uv: vec2f) -> @location(0) vec4f {
    let a = textureSample(albedoTex, samp, uv);
    let n = normalize(textureSample(normalTex, samp, uv).xyz * 2.0 - 1.0);
    let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    let world = vec2f(ndc.x * lights.header.y, ndc.y);
    var col = shade(a.rgb, n, world, lights);
    col = col + a.rgb * a.a;
    return vec4f(col, 1.0);
}
)WGSL";
}

// Forward-пасс прозрачного спрайта — ВТОРОЙ потребитель того же lighting_module_wgsl().
// Тот же shade(), что и deferred: доказывает, что шов technique держит второго потребителя.
std::string forward_wgsl() {
    return lighting_module_wgsl() + R"WGSL(
struct SpriteU {
    pos: vec2f, scale: f32, rot: f32, aspect: f32, alpha: f32, emissive: f32, _pad: f32,
};
@group(0) @binding(0) var<uniform> u: SpriteU;
@group(0) @binding(1) var samp: sampler;
@group(0) @binding(2) var albedoTex: texture_2d<f32>;
@group(0) @binding(3) var normalTex: texture_2d<f32>;
@group(0) @binding(4) var<uniform> lights: LightsU;

struct VSOut {
    @builtin(position) clip: vec4f,
    @location(0) uv: vec2f,
    @location(1) world: vec2f,
};

@vertex
fn vs(@location(0) pos: vec2f, @location(1) uv: vec2f) -> VSOut {
    var p = pos * u.scale;
    let c = cos(u.rot);
    let s = sin(u.rot);
    p = vec2f(p.x * c - p.y * s, p.x * s + p.y * c);
    p += u.pos;
    var o: VSOut;
    o.world = p;
    o.clip = vec4f(p.x / u.aspect, p.y, 0.0, 1.0);
    o.uv = uv;
    return o;
}

@fragment
fn fs(in: VSOut) -> @location(0) vec4f {
    let a = textureSample(albedoTex, samp, in.uv);
    let n = normalize(textureSample(normalTex, samp, in.uv).xyz * 2.0 - 1.0);
    var col = shade(a.rgb, n, in.world, lights);
    col = col + a.rgb * u.emissive;
    return vec4f(col, a.a * u.alpha);
}
)WGSL";
}
