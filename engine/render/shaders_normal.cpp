#include "shaders_normal.hpp"

// Проход нормалей рисует ТЕ ЖЕ квады, что и проход материалов, но пишет не цвет, а нормаль
// поверхности из текстурного слота материала. Пайплайн один на все материалы: отличает их
// привязанная карта, то есть ДАННЫЕ, а не точка входа.
//
// Альфа спрайта — покрытие: за кромкой круга нормали нет, и туда обязана остаться плоская нормаль
// очистки. Смешивание по альфе делает это одним правилом вместо `discard`, который на кромке дал
// бы ступеньку вместо перехода.
const char* normal_pass_wgsl() {
    // ascii: allow комментарии внутри исходника шейдера
    return R"WGSL(
struct Viewport { half_extent: vec2f, texel: vec2f };
@group(0) @binding(0) var<uniform> vp: Viewport;
@group(0) @binding(1) var samp: sampler;
@group(0) @binding(2) var albedoTex: texture_2d<f32>;
@group(0) @binding(3) var normalTex: texture_2d<f32>;

struct VsIn {
    @location(0) qpos: vec2f,
    @location(1) quv: vec2f,
    @location(2) ipos: vec2f,
    @location(3) isize: vec2f,
    @location(4) iuv0: vec2f,
    @location(5) iuv1: vec2f,
    @location(7) irot: f32,
};
struct VsOut { @builtin(position) pos: vec4f, @location(0) uv: vec2f };

@vertex
fn vs_main(in: VsIn) -> VsOut {
    let k = cos(in.irot);
    let s = sin(in.irot);
    let rq = vec2f(in.qpos.x * k - in.qpos.y * s, in.qpos.x * s + in.qpos.y * k);
    var out: VsOut;
    out.pos = vec4f((in.ipos + rq * in.isize) / vp.half_extent, 0.0, 1.0);
    out.uv = mix(in.iuv0, in.iuv1, in.quv);
    return out;
}

@fragment
fn fs_normal(in: VsOut) -> @location(0) vec4f {
    let cover = textureSample(albedoTex, samp, in.uv).a;
    let n = textureSample(normalTex, samp, in.uv).xyz;
    return vec4f(n, cover);
}
)WGSL";
}
