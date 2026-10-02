#include <cstdio>
#include <cstring>
#include <string>

#include "cel_quads.hpp"
#include "clip_golden.hpp"
#include "golden_target.hpp"
#include "gpu.hpp"
#include "platform_args.hpp"
#include "render_capture.hpp"

// Гейт В6б спеки #24 на трёх ОС. Контроли сверх вердикта: эталон без флипа и эталон без боксов
// обязаны разойтись (сцена видит и то и другое), последний тик кадра и первый тик следующего —
// различаться, повтор — совпасть, sRGB-цель — лечь теми же байтами.
namespace {

using namespace clip_golden;

constexpr uint32_t CAP = 256;
DebugQuad overlay[CAP];
render::Quad quads[CAP];

int fails = 0;

void check(bool ok, const char* what) {
    if (ok) return;
    std::printf("clip-golden: FAIL: %s\n", what);
    ++fails;
}

DebugGlyphs solid_glyphs() {
    DebugGlyphs g;
    g.has_solid = true;
    return g;
}

struct Built {
    uint16_t frame = 0;
    uint32_t quads = 0;
    render::QuadRun runs[2];
};

Built build(const Scene& s, const View& v) {
    Built b;
    b.frame = clip_frame_at(s.clip.clip, v.tick);
    const ClipCel* cel = frame_cel(s.clip, b.frame);
    check(cel != nullptr && cel_quad(*cel, v.at, {s.sheet.w, s.sheet.h}, quads[0]), "cel lands on the sheet");
    DebugDraw dd(overlay, CAP, solid_glyphs());
    draw_cel_debug(dd, s.clip, b.frame, v.at);
    const DebugQuadStats st = debug_quads({overlay, dd.count()}, {quads + 1, CAP - 1});
    check(dd.dropped() == 0 && st.rejected == 0 && st.dropped == 0, "overlay fits the buffers");
    b.quads = 1 + st.quads;
    b.runs[0] = {0, 1, 0};
    b.runs[1] = {1, st.quads, 1};
    return b;
}

std::vector<uint8_t> frame_of(GpuContext& gpu, render::QuadRenderer& qr, const Scene& s, const View& v,
                              WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm) {
    const Built b = build(s, v);
    check(qr.upload({quads, b.quads}, W, H), "quads uploaded");
    return golden_target::render(gpu, qr, b.runs, W, H, CLEAR, format);
}

std::vector<uint8_t> shoot(GpuContext& gpu, render::QuadRenderer& qr, const Scene& s, const View& v) {
    std::vector<uint8_t> px = frame_of(gpu, qr, s, v);
    const uint32_t bad = golden_target::mismatched(px, reference(s, v, Blind::None));
    const uint16_t frame = clip_frame_at(s.clip.clip, v.tick);
    std::printf("clip-golden: %s tick %llu frame %u zoom %d flip %d: %zu hit, %zu hurt, %zu push, %u mismatched\n",
                v.name, static_cast<unsigned long long>(v.tick), frame, v.at.zoom, v.at.flip_h ? 1 : 0,
                frame_boxes(s.clip, frame, BoxKind::Hit).size(), frame_boxes(s.clip, frame, BoxKind::Hurt).size(),
                frame_boxes(s.clip, frame, BoxKind::Push).size(), bad);
    check(bad == 0, "GPU frame equals the CPU reference");
    return px;
}

void quads_refuse(const Scene& s) {
    render::Quad q;
    const bool past = cel_quad(s.clip.cels[2], VIEWS[0].at, {s.sheet.w - 1, s.sheet.h}, q);
    DebugDraw dd(overlay, CAP, solid_glyphs());
    dd.line({fix32::from_int(0), fix32::from_int(0)}, {fix32::from_int(8), fix32::from_int(8)}, fix32::from_int(1),
            0xffffffffu);
    draw_cel_debug(dd, s.clip, 2, VIEWS[0].at);
    const DebugQuadStats st = debug_quads({overlay, dd.count()}, {quads, 3});
    std::printf("clip-golden: quads: cel past a narrowed sheet %s, %u rejected rotated, %u dropped past 3\n",
                past ? "accepted" : "refused", st.rejected, st.dropped);
    check(!past && st.rejected == 1 && st.dropped > 0 && st.quads == 3, "quads refuse what does not fit");
}

uint32_t srgb_target(GpuContext& gpu, const Scene& s, const std::vector<uint8_t>& unorm) {
    render::QuadRenderer qs;
    check(qs.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA8UnormSrgb, CAP), "sRGB renderer");
    check(qs.add_texture(s.sheet.rgba.data(), s.sheet.w, s.sheet.h), "sheet");
    check(qs.add_texture(s.solid.rgba.data(), 1, 1), "solid");
    const uint32_t bad =
        golden_target::mismatched(unorm, frame_of(gpu, qs, s, VIEWS[2], WGPUTextureFormat_RGBA8UnormSrgb));
    std::printf("clip-golden: srgb: %u pixel(s) differ on an sRGB target\n", bad);
    return bad;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const char* out_dir = nullptr;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--out") == 0) out_dir = argv[i + 1];
    Scene s;
    if (!build_scene(s)) {
        std::printf("clip-golden: FAIL: scene does not bake\n");
        return 1;
    }
    quads_refuse(s);
    GpuContext gpu;
    if (!gpu.init(nullptr)) {
        std::printf("clip-golden: FAIL: no GPU adapter\n");
        return 1;
    }
    render::QuadRenderer qr;
    if (!qr.init(gpu.device, gpu.queue, WGPUTextureFormat_RGBA8Unorm, CAP)) {
        std::printf("clip-golden: FAIL: quad renderer\n");
        qr.shutdown();
        gpu.shutdown();
        return 1;
    }
    check(qr.add_texture(s.sheet.rgba.data(), s.sheet.w, s.sheet.h), "sheet");
    check(qr.add_texture(s.solid.rgba.data(), 1, 1), "solid");
    std::vector<uint8_t> shots[3];
    for (uint32_t i = 0; i < 3; ++i) {
        shots[i] = shoot(gpu, qr, s, VIEWS[i]);
        if (out_dir != nullptr)
            capture::write_png((std::string(out_dir) + "/clip_" + VIEWS[i].name + ".png").c_str(), shots[i], W, H);
    }
    const uint32_t flip_blind = golden_target::mismatched(shots[2], reference(s, VIEWS[2], Blind::Flip));
    const uint32_t box_blind = golden_target::mismatched(shots[0], reference(s, VIEWS[0], Blind::Boxes));
    const uint32_t edge = golden_target::mismatched(shots[0], shots[1]);
    const uint32_t repeat = golden_target::mismatched(shots[2], frame_of(gpu, qr, s, VIEWS[2]));
    std::printf("clip-golden: control: flip-blind reference %u, box-blind reference %u mismatched\n", flip_blind,
                box_blind);
    std::printf("clip-golden: edge: ticks 2 and 3 differ in %u pixel(s)\n", edge);
    std::printf("clip-golden: repeat: %u pixel(s) differ between two runs\n", repeat);
    check(flip_blind > 0, "flip-blind reference diverges");
    check(box_blind > 0, "box-blind reference diverges");
    check(edge > 0, "the frame changes on its first tick");
    check(repeat == 0, "two runs agree");
    check(srgb_target(gpu, s, shots[2]) == 0, "sRGB target keeps the texel and overlay bytes");
    qr.shutdown();
    gpu.shutdown();
    if (fails != 0) return 1;
    std::printf("clip-golden: PASS\n");
    return 0;
}
