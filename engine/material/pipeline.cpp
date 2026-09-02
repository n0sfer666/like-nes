#include "pipeline.hpp"

#include <cctype>
#include <cstring>

#include "instance.hpp"
#include "table.hpp"

namespace mat::detail {
namespace {

void set_blend(uint8_t blend, WGPUBlendState& out) {
    out.color.operation = WGPUBlendOperation_Add;
    out.alpha.operation = WGPUBlendOperation_Add;
    out.alpha.srcFactor = WGPUBlendFactor_One;
    out.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    switch (static_cast<Blend>(blend)) {
        case Blend::Alpha:
            out.color.srcFactor = WGPUBlendFactor_SrcAlpha;
            out.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
            break;
        case Blend::Additive:
            out.color.srcFactor = WGPUBlendFactor_SrcAlpha;
            out.color.dstFactor = WGPUBlendFactor_One;
            out.alpha.dstFactor = WGPUBlendFactor_One;
            break;
        case Blend::Opaque:
        default:
            out.color.srcFactor = WGPUBlendFactor_One;
            out.color.dstFactor = WGPUBlendFactor_Zero;
            out.alpha.dstFactor = WGPUBlendFactor_Zero;
            break;
    }
}

} // namespace

WGPUShaderModule make_module(WGPUDevice device, const char* wgsl) {
    WGPUShaderModuleWGSLDescriptor desc = {};
    desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    desc.code = wgsl;
    WGPUShaderModuleDescriptor sd = {};
    sd.nextInChain = &desc.chain;
    return wgpuDeviceCreateShaderModule(device, &sd);
}

// Слева от имени стоит `fn` и ничего кроме пробелов: искалось ОБЪЯВЛЕНИЕ, а совпадение в
// комментарии или в вызове (`// fs_flash (устарело)`) отдавало true — кэш шёл компилировать
// несуществующую точку входа, и в stderr печаталось «pipeline refused by the validator» вместо
// «no entry point in the library». Диагностика называла не того виновника.
bool preceded_by_fn(const char* wgsl, const char* p) {
    while (p > wgsl && (p[-1] == ' ' || p[-1] == '\t')) --p;
    if (p - wgsl < 2 || p[-1] != 'n' || p[-2] != 'f') return false;
    const char* before = p - 2;
    return before == wgsl || (!std::isalnum(static_cast<unsigned char>(before[-1])) &&
                              before[-1] != '_');
}

bool has_entry(const char* wgsl, const char* entry) {
    if (!wgsl || !entry || !*entry) return false;
    const std::size_t n = std::strlen(entry);
    for (const char* p = std::strstr(wgsl, entry); p; p = std::strstr(p + 1, entry)) {
        // Имя обязано быть ЦЕЛЫМ словом: `fs_flash` встречается внутри `fs_flash_extra`, и
        // подстрока молча превратила бы опечатку в чужой эффект.
        const bool left = p == wgsl || (!std::isalnum(static_cast<unsigned char>(p[-1])) &&
                                        p[-1] != '_');
        const bool right = p[n] == '(' || p[n] == ' ';
        if (left && right && preceded_by_fn(wgsl, p)) return true;
    }
    return false;
}

WGPURenderPipeline make_pipeline(WGPUDevice device, WGPUPipelineLayout layout,
                                 WGPUShaderModule module, const char* entry, uint8_t blend,
                                 WGPUTextureFormat target) {
    WGPUVertexAttribute quad[2] = {};
    quad[0].format = WGPUVertexFormat_Float32x2; quad[0].offset = 0;  quad[0].shaderLocation = 0;
    quad[1].format = WGPUVertexFormat_Float32x2; quad[1].offset = 8;  quad[1].shaderLocation = 1;

    WGPUVertexAttribute inst[8] = {};
    inst[0].format = WGPUVertexFormat_Float32x2; inst[0].offset = 0;  inst[0].shaderLocation = 2;
    inst[1].format = WGPUVertexFormat_Float32x2; inst[1].offset = 8;  inst[1].shaderLocation = 3;
    inst[2].format = WGPUVertexFormat_Float32x2; inst[2].offset = 16; inst[2].shaderLocation = 4;
    inst[3].format = WGPUVertexFormat_Float32x2; inst[3].offset = 24; inst[3].shaderLocation = 5;
    inst[4].format = WGPUVertexFormat_Float32x4; inst[4].offset = 32; inst[4].shaderLocation = 6;
    inst[5].format = WGPUVertexFormat_Float32;   inst[5].offset = 48; inst[5].shaderLocation = 7;
    inst[6].format = WGPUVertexFormat_Float32x4; inst[6].offset = 52; inst[6].shaderLocation = 8;
    inst[7].format = WGPUVertexFormat_Float32x4; inst[7].offset = 68; inst[7].shaderLocation = 9;
    static_assert(sizeof(Instance) == 84, "vertex attribute offsets above read this layout");

    WGPUVertexBufferLayout vbl[2] = {};
    vbl[0].arrayStride = 16; vbl[0].stepMode = WGPUVertexStepMode_Vertex;
    vbl[0].attributeCount = 2; vbl[0].attributes = quad;
    vbl[1].arrayStride = sizeof(Instance); vbl[1].stepMode = WGPUVertexStepMode_Instance;
    vbl[1].attributeCount = 8; vbl[1].attributes = inst;

    WGPUBlendState bs = {};
    set_blend(blend, bs);
    WGPUColorTargetState ct = {};
    ct.format = target; ct.blend = &bs; ct.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState fs = {};
    fs.module = module; fs.entryPoint = entry; fs.targetCount = 1; fs.targets = &ct;

    WGPURenderPipelineDescriptor rp = {};
    rp.layout = layout;
    rp.vertex.module = module; rp.vertex.entryPoint = "vs_main";
    rp.vertex.bufferCount = 2; rp.vertex.buffers = vbl;
    rp.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rp.primitive.cullMode = WGPUCullMode_None;
    rp.multisample.count = 1; rp.multisample.mask = 0xFFFFFFFF;
    rp.fragment = &fs;
    return wgpuDeviceCreateRenderPipeline(device, &rp);
}

const char* fallback_wgsl() {
    return R"(
struct Viewport { half_extent: vec2f, texel: vec2f };
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

// Marker colour, not a copy of the sprite: a fallback that looks like the real thing hides the
// very failure it stands in for.
@fragment
fn fs_fallback(in: VsOut) -> @location(0) vec4f {
    let checker = (floor(in.uv.x * 8.0) + floor(in.uv.y * 8.0)) % 2.0;
    return vec4f(1.0, checker * 0.4, 1.0, 1.0);
}
)";
}

} // namespace mat::detail
