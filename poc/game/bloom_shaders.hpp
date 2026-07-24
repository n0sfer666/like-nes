#pragma once

// WGSL пост-процесс-шейдеры bloom (S9, техника из спеки #2 render/shaders_post.cpp).
// Вынесено из bloom.cpp (лимит 200 строк на файл).
namespace game {

// Fullscreen-треугольник (без вершинных буферов).
inline const char* BLOOM_VS = R"(
struct VOut { @builtin(position) pos: vec4f, @location(0) uv: vec2f };
@vertex fn vs(@builtin(vertex_index) i: u32) -> VOut {
  var p = array<vec2f,3>(vec2f(-1.0,-1.0), vec2f(3.0,-1.0), vec2f(-1.0,3.0));
  var o: VOut;
  o.pos = vec4f(p[i], 0.0, 1.0);
  o.uv = p[i] * vec2f(0.5, -0.5) + vec2f(0.5, 0.5);
  return o;
}
)";

inline const char* BLOOM_BRIGHT_FS = R"(
@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var hdrTex: texture_2d<f32>;
@fragment fn fs(@location(0) uv: vec2f) -> @location(0) vec4f {
  let c = textureSample(hdrTex, samp, uv).rgb;
  let l = dot(c, vec3f(0.2126, 0.7152, 0.0722));
  let knee = max(l - 1.0, 0.0) / max(l, 1e-4);
  return vec4f(c * knee, 1.0);
}
)";

inline const char* BLOOM_BLUR_FS = R"(
struct BlurU { dir: vec2f, pad: vec2f };
@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var srcTex: texture_2d<f32>;
@group(0) @binding(2) var<uniform> b: BlurU;
@fragment fn fs(@location(0) uv: vec2f) -> @location(0) vec4f {
  var w = array<f32, 5>(0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
  var acc = textureSample(srcTex, samp, uv).rgb * w[0];
  for (var i = 1; i < 5; i = i + 1) {
    let o = b.dir * f32(i);
    acc = acc + textureSample(srcTex, samp, uv + o).rgb * w[i];
    acc = acc + textureSample(srcTex, samp, uv - o).rgb * w[i];
  }
  return vec4f(acc, 1.0);
}
)";

inline const char* BLOOM_TONE_FS = R"(
@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var hdrTex: texture_2d<f32>;
@group(0) @binding(2) var bloomTex: texture_2d<f32>;
fn aces(x: vec3f) -> vec3f {
  let a = 2.51; let b = 0.03; let c = 2.43; let d = 0.59; let e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3f(0.0), vec3f(1.0));
}
@fragment fn fs(@location(0) uv: vec2f) -> @location(0) vec4f {
  var col = textureSample(hdrTex, samp, uv).rgb;
  col = col + textureSample(bloomTex, samp, uv).rgb * 0.75;
  return vec4f(aces(col), 1.0);
}
)";

} // namespace game
