#include "shaders_slot.hpp"

// Проход слота рисует ТЕ ЖЕ квады, что и проход материалов, но пишет не цвет, а величину из
// текстурного слота материала. Вершинная функция одна на оба буфера, фрагментных две: буфер
// выбирается ТОЧКОЙ ВХОДА из `Desc`, а материалы внутри буфера отличает привязанная карта, то
// есть данные, а не ветка.
//
// Альфа спрайта — покрытие: за кромкой круга ни нормали, ни перекрытия нет, и туда обязано
// остаться значение очистки. Смешивание по альфе делает это одним правилом вместо `discard`,
// который на кромке дал бы ступеньку вместо перехода.
const char* slot_pass_wgsl() {
    // ascii: allow комментарии внутри исходника шейдера
    return R"WGSL(
struct Viewport { half_extent: vec2f, texel: vec2f };
@group(0) @binding(0) var<uniform> vp: Viewport;
@group(0) @binding(1) var samp: sampler;
@group(0) @binding(2) var albedoTex: texture_2d<f32>;
@group(0) @binding(3) var slotTex: texture_2d<f32>;

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
    let n = textureSample(slotTex, samp, in.uv).xyz;
    return vec4f(n, cover);
}

// Перекрытие живёт в канале R по контракту `slot_textures.hpp`: 0 не перекрывает ничего, 1 —
// целиком. Пишется во все три канала, чтобы буфер читался глазами при отладке тем же просмотрщиком.
@fragment
fn fs_occluder(in: VsOut) -> @location(0) vec4f {
    let cover = textureSample(albedoTex, samp, in.uv).a;
    let o = textureSample(slotTex, samp, in.uv).r;
    return vec4f(o, o, o, cover);
}
)WGSL";
}
