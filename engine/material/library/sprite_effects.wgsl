// Effect library of the engine (spec #18, decision 4: the library is first-class, not examples).
//
// ONE vertex stage for the whole library and one fragment entry per effect. The material names the
// FRAGMENT entry; the vertex module is shared, so adding an effect adds one pipeline, not two
// modules. Each entry is baked as its own asset by `assetc` (`bakers::shader` takes an entry point),
// which is why they live in one file instead of three copies of the same vertex stage.
//
// Parameter slots are NOT free-form: the baker assigns them in declaration order in `library.mat`,
// and the numbers below mirror that assignment. The mirror is pinned by `material_library_test` --
// reordering the rows of `library.mat` without touching this file fails that gate instead of
// silently feeding the shader a value from the wrong offset.

struct Viewport {
    // Half the screen in pixels: world coordinates divide by it and land in NDC. Same shape and
    // size as the uniform of the sample game's SpriteBatch, so the library has ONE vertex stage
    // across both consumers instead of a copy per consumer.
    half_extent: vec2f,
    // Padding to the 16 bytes a uniform buffer binds, and the same shape as the sample game's VP
    // struct. Effects that need a size work in the INSTANCE's units instead (see `upx` below):
    // a screen-wide texel says nothing about how large this particular sprite is drawn.
    pad: vec2f,
};

@group(0) @binding(0) var<uniform> vp: Viewport;
@group(0) @binding(1) var samp: sampler;
@group(0) @binding(2) var albedo: texture_2d<f32>;
@group(0) @binding(3) var aux: texture_2d<f32>;

struct VsIn {
    @location(0) qpos: vec2f,
    @location(1) quv: vec2f,
    @location(2) ipos: vec2f,
    @location(3) isize: vec2f,
    @location(4) iuv0: vec2f,
    @location(5) iuv1: vec2f,
    @location(6) itint: vec4f,
    @location(7) irot: f32,
    @location(8) ip0: vec4f,
    @location(9) ip1: vec4f,
};

struct VsOut {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
    @location(2) p0: vec4f,
    @location(3) p1: vec4f,
    // How much UV one SCREEN PIXEL of this instance covers. Thickness authored in pixels becomes a
    // UV offset only through this: the same material on a sprite drawn twice as large must keep the
    // outline one pixel wide, and a constant taken from the screen or the atlas cannot do that.
    @location(4) upx: vec2f,
};

@vertex
fn vs_main(in: VsIn) -> VsOut {
    let k = cos(in.irot);
    let s = sin(in.irot);
    let rq = vec2f(in.qpos.x * k - in.qpos.y * s, in.qpos.x * s + in.qpos.y * k);
    let world = in.ipos + rq * in.isize;

    var out: VsOut;
    out.pos = vec4f(world / vp.half_extent, 0.0, 1.0);
    out.uv = mix(in.iuv0, in.iuv1, in.quv);
    out.color = in.itint;
    out.p0 = in.ip0;
    out.p1 = in.ip1;
    out.upx = (in.iuv1 - in.iuv0) / in.isize;
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
    let step = in.upx * in.p1.x;
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
