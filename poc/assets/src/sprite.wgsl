// Authoring-исходник спрайт-шейдера. Бейкается Tint'ом в SPIR-V backend-IR (per-stage),
// шипается в бандл, грузится рантаймом zero-copy в WGPUShaderModuleSPIRVDescriptor.
// Минимальный textured-quad: доказывает шов asset->render (спека #5 гейт) и шейдер-кеш (спека #2).

struct Uniforms {
    offset: vec2f,
    scale: vec2f,
};
@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var albedo: texture_2d<f32>;
@group(0) @binding(2) var<uniform> u: Uniforms;

struct VsOut {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vs_main(@builtin(vertex_index) vi: u32) -> VsOut {
    var quad = array<vec2f, 4>(
        vec2f(-0.5, -0.5), vec2f(0.5, -0.5), vec2f(0.5, 0.5), vec2f(-0.5, 0.5)
    );
    var idx = array<u32, 6>(0u, 1u, 2u, 0u, 2u, 3u);
    let p = quad[idx[vi]];
    var out: VsOut;
    out.pos = vec4f(p * u.scale + u.offset, 0.0, 1.0);
    out.uv = p + vec2f(0.5, 0.5);
    return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
    let c = textureSample(albedo, samp, in.uv);
    return vec4f(c.rgb, c.a);
}
