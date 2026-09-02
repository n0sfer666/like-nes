// Effect library of the engine (spec #18, decision 4: the library is first-class, not examples).
//
// ONE vertex stage for the whole library and one fragment entry per effect. The material names the
// FRAGMENT entry; the vertex module is shared, so adding an effect adds one pipeline, not two
// modules. Each entry is baked as its own asset by `assetc` (`bakers::shader` takes an entry point),
// which is why they live in one file instead of three copies of the same vertex stage.
//
// Parameter slots are NOT free-form: the baker assigns them in declaration order in `library.mat`,
// and the numbers below mirror that assignment. The mirror is pinned by `material_library_test` —
// reordering the rows of `library.mat` without touching this file fails that gate instead of
// silently feeding the shader a value from the wrong offset.

struct Viewport {
    size: vec2f,
    texel: vec2f,
};

@group(0) @binding(0) var<uniform> vp: Viewport;
@group(0) @binding(1) var samp: sampler;
@group(0) @binding(2) var albedo: texture_2d<f32>;
@group(0) @binding(3) var aux: texture_2d<f32>;

struct VsIn {
    @builtin(vertex_index) vi: u32,
    @location(0) rect: vec4f,
    @location(1) uv: vec4f,
    @location(2) color: vec4f,
    @location(3) rot: f32,
    @location(4) p0: vec4f,
    @location(5) p1: vec4f,
};

struct VsOut {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
    @location(2) p0: vec4f,
    @location(3) p1: vec4f,
};

@vertex
fn vs_main(in: VsIn) -> VsOut {
    var corner = array<vec2f, 4>(
        vec2f(-0.5, -0.5), vec2f(0.5, -0.5), vec2f(0.5, 0.5), vec2f(-0.5, 0.5)
    );
    var idx = array<u32, 6>(0u, 1u, 2u, 0u, 2u, 3u);
    let c = corner[idx[in.vi]];

    let s = sin(in.rot);
    let k = cos(in.rot);
    let scaled = vec2f(c.x * in.rect.z, c.y * in.rect.w);
    let rotated = vec2f(scaled.x * k - scaled.y * s, scaled.x * s + scaled.y * k);
    let px = in.rect.xy + rotated;

    var out: VsOut;
    // +Y is down in engine units (physics/units.hpp), +Y is up in clip space: the flip lives here,
    // once, and not in every effect.
    out.pos = vec4f(px.x / vp.size.x * 2.0 - 1.0, 1.0 - px.y / vp.size.y * 2.0, 0.0, 1.0);
    out.uv = mix(in.uv.xy, in.uv.zw, c + vec2f(0.5, 0.5));
    out.color = in.color;
    out.p0 = in.p0;
    out.p1 = in.p1;
    return out;
}

// flash: p0 = tint colour, p1.x = strength (fraction).
@fragment
fn fs_flash(in: VsOut) -> @location(0) vec4f {
    let c = textureSample(albedo, samp, in.uv) * in.color;
    return vec4f(mix(c.rgb, in.p0.rgb, in.p1.x * in.p0.a), c.a);
}

// outline: p0 = outline colour, p1.x = thickness in pixels.
// The ring is drawn OUTSIDE the sprite (where its own alpha is zero) so the silhouette is not
// eaten by the outline as thickness grows.
@fragment
fn fs_outline(in: VsOut) -> @location(0) vec4f {
    let c = textureSample(albedo, samp, in.uv) * in.color;
    if (c.a > 0.5) {
        return c;
    }
    let step = vp.texel * in.p1.x;
    var near = 0.0;
    near = max(near, textureSample(albedo, samp, in.uv + vec2f(step.x, 0.0)).a);
    near = max(near, textureSample(albedo, samp, in.uv - vec2f(step.x, 0.0)).a);
    near = max(near, textureSample(albedo, samp, in.uv + vec2f(0.0, step.y)).a);
    near = max(near, textureSample(albedo, samp, in.uv - vec2f(0.0, step.y)).a);
    near = max(near, textureSample(albedo, samp, in.uv + step).a);
    near = max(near, textureSample(albedo, samp, in.uv - step).a);
    near = max(near, textureSample(albedo, samp, in.uv + vec2f(step.x, -step.y)).a);
    near = max(near, textureSample(albedo, samp, in.uv + vec2f(-step.x, step.y)).a);
    if (near <= 0.5) {
        return vec4f(0.0, 0.0, 0.0, 0.0);
    }
    return vec4f(in.p0.rgb, in.p0.a);
}

// dissolve: p0 = edge colour, p1.x = threshold (fraction), p1.y = edge width (fraction).
// The colour comes first because the baker packs a vec4 into the lowest free run of four: declared
// after the two scalars it would straddle p0.zw and p1.xy, and a vec4 split across two attributes
// is a trap for the next effect written by hand.
// The noise comes from the aux slot, so the same shader dissolves differently per material
// instance without a second pipeline.
@fragment
fn fs_dissolve(in: VsOut) -> @location(0) vec4f {
    let c = textureSample(albedo, samp, in.uv) * in.color;
    let n = textureSample(aux, samp, in.uv).r;
    if (n < in.p1.x) {
        return vec4f(0.0, 0.0, 0.0, 0.0);
    }
    if (n < in.p1.x + in.p1.y) {
        return vec4f(in.p0.rgb, c.a * in.p0.a);
    }
    return c;
}
