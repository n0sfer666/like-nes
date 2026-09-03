#include "shaders_light.hpp"

// Проход освещения читает источники из storage-буфера, а их число берёт у `arrayLength`. Так
// «сколько светов» перестаёт быть свойством ШЕЙДЕРА: прежний `LightsU` держал `array<vec4f, 6>`,
// и седьмой источник требовал правки WGSL, пересборки движка и нового пина ABI.
//
// Контракт знака направления записан здесь и в `light/table.hpp`, а не подразумевается: `dir` —
// направление, КУДА светит источник, поэтому вектор НА свет равен `-dir`. Незаписанное соглашение
// о знаке каждый потребитель пишет по-своему, и расходятся они молча — светом с изнанки.
const char* light_pass_wgsl() {
    // ascii: allow комментарии внутри исходника шейдера
    return R"WGSL(
struct Light {
    pos_h: vec4f,    // xy — позиция в мире, z — высота над плоскостью, w — радиус
    color_i: vec4f,  // rgb — линейный цвет, w — сила
    dir_k: vec4f,    // xy — единичное направление, z — вид (0 точечный, 1 направленный)
};
struct Frame {
    ambient: vec4f,  // rgb — цвет фоновой засветки, w — её сила
    view: vec4f,     // x — аспект кадра
};

@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var albedoTex: texture_2d<f32>;
@group(0) @binding(2) var normalTex: texture_2d<f32>;
@group(0) @binding(3) var<storage, read> lights: array<Light>;
@group(0) @binding(4) var<uniform> frame: Frame;

@fragment
fn fs(@location(0) uv: vec2f) -> @location(0) vec4f {
    let a = textureSample(albedoTex, samp, uv);
    let n = normalize(textureSample(normalTex, samp, uv).xyz * 2.0 - 1.0);
    let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    let world = vec2f(ndc.x * frame.view.x, ndc.y);

    var lit = a.rgb * frame.ambient.rgb * frame.ambient.w;
    let count = arrayLength(&lights);
    for (var i = 0u; i < count; i = i + 1u) {
        let L = lights[i];
        var toL = vec3f(0.0, 0.0, 1.0);
        var atten = 1.0;
        if (L.dir_k.z > 0.5) {
            // У направленного затухания нет: он одинаков во всём кадре, а высоту заменяет
            // фиксированный подъём над плоскостью — иначе при плоской нормали он не светит вовсе.
            toL = vec3f(-L.dir_k.xy, 1.0);
        } else {
            let v = vec3f(L.pos_h.xy - world, L.pos_h.z);
            let d = length(v);
            toL = v;
            atten = pow(clamp(1.0 - d / max(L.pos_h.w, 1e-4), 0.0, 1.0), 2.0);
        }
        let Ln = normalize(toL);
        let ndl = max(dot(n, Ln), 0.0);
        lit = lit + a.rgb * L.color_i.rgb * (L.color_i.w * ndl * atten);
    }
    return vec4f(lit, 1.0);
}
)WGSL";
}
