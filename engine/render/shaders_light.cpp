#include "shaders_light.hpp"

// Проход освещения читает источники из storage-буфера, а их число берёт у `arrayLength`. Так
// «сколько светов» перестаёт быть свойством ШЕЙДЕРА: прежний `LightsU` держал `array<vec4f, 6>`,
// и седьмой источник требовал правки WGSL, пересборки движка и нового пина ABI.
//
// Контракт знака направления записан здесь и в `light/table.hpp`, а не подразумевается: `dir` —
// направление, КУДА светит источник, поэтому вектор НА свет равен `-dir`. Незаписанное соглашение
// о знаке каждый потребитель пишет по-своему, и расходятся они молча — светом с изнанки.
//
// Тени (шаг C): экранный марш ОТ затеняемой точки К источнику по буферу перекрытия. Мягкость
// каждого источника — поле его строки таблицы (`dir_k.w`), а не константа шейдера: с константой
// «мягкость — данные» нечем отличить от «у всех одинаково». Тень множит только ПРЯМОЙ вклад
// источника: фоновая засветка приходит отовсюду, и затенять её значило бы гасить кадр целиком.
const char* light_pass_wgsl() {
    // ascii: allow комментарии внутри исходника шейдера
    return R"WGSL(
struct Light {
    pos_h: vec4f,    // xy — позиция в мире, z — высота над плоскостью, w — радиус
    color_i: vec4f,  // rgb — линейный цвет, w — сила
    dir_k: vec4f,    // xy — единичное направление, z — вид (0 точечный, 1 направленный),
                     // w — мягкость тени в мире (0 — резкая кромка)
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
@group(0) @binding(5) var occTex: texture_2d<f32>;

// Длина луча — в мире, а не в текселях: спрайт сцены равен 0.36 мира, и тень длиной в полтора
// спрайта видна, но не заливает кадр. Шаги считаны так же грубо: марш — приближение, и его цена
// названа `report_cost`, а не спрятана.
const MARCH_STEPS: i32 = 8;
const MARCH_MAX: f32 = 0.45;

fn uv_of(world: vec2f, aspect: f32) -> vec2f {
    let ndc = vec2f(world.x / aspect, world.y);
    return vec2f(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
}

// Возвращает МНОЖИТЕЛЬ прямого вклада: 1 — свет дошёл, 0 — перекрыт целиком.
fn reach_of(p: vec2f, toL: vec2f, soft: f32, aspect: f32) -> f32 {
    let len = length(toL);
    if (len < 1e-5) { return 1.0; }
    let dirw = toL / len;
    let reach = min(len, MARCH_MAX);
    let perp = vec2f(-dirw.y, dirw.x);
    // Перекрытие ПОД самой точкой вычитается: иначе перекрыватель затеняет сам себя целиком, и
    // «тень» получается ровной заливкой спрайта, неотличимой от потемневшего материала.
    let here = textureSample(occTex, samp, uv_of(p, aspect)).r;
    var acc = 0.0;
    for (var i = 1; i <= MARCH_STEPS; i = i + 1) {
        let t = f32(i) / f32(MARCH_STEPS);
        let s = p + dirw * (reach * t);
        // Полутень шире у ИСТОЧНИКА, а не у приёмника: смещение растёт с долей пройденного пути.
        let off = perp * (soft * t);
        let a = textureSample(occTex, samp, uv_of(s - off, aspect)).r;
        let b = textureSample(occTex, samp, uv_of(s, aspect)).r;
        let c = textureSample(occTex, samp, uv_of(s + off, aspect)).r;
        acc = max(acc, clamp((a + b + c) / 3.0 - here, 0.0, 1.0));
    }
    return 1.0 - acc;
}

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
        var flat_to_light = vec2f(0.0, 0.0);
        if (L.dir_k.z > 0.5) {
            // У направленного затухания нет: он одинаков во всём кадре, а высоту заменяет
            // фиксированный подъём над плоскостью — иначе при плоской нормали он не светит вовсе.
            toL = vec3f(-L.dir_k.xy, 1.0);
            flat_to_light = -L.dir_k.xy * MARCH_MAX;
        } else {
            let v = vec3f(L.pos_h.xy - world, L.pos_h.z);
            let d = length(v);
            toL = v;
            flat_to_light = L.pos_h.xy - world;
            atten = pow(clamp(1.0 - d / max(L.pos_h.w, 1e-4), 0.0, 1.0), 2.0);
        }
        let Ln = normalize(toL);
        let ndl = max(dot(n, Ln), 0.0);
        let reach = reach_of(world, flat_to_light, L.dir_k.w, frame.view.x);
        lit = lit + a.rgb * L.color_i.rgb * (L.color_i.w * ndl * atten * reach);
    }
    return vec4f(lit, 1.0);
}
)WGSL";
}
