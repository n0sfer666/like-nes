#include "light_checks.hpp"

#include <chrono>
#include <cstdio>

#include "../light/bake.hpp"
#include "gpu.hpp"
#include "light_frame.hpp"

namespace lightgold {
namespace {

constexpr int COST_FRAMES = 30;

int fails = 0;

// Два набора, отличающиеся ТОЛЬКО числом источников.
const char* const TWO_LIGHTS =
    "ambient | 0.10, 0.12, 0.18 | 0.35\n"
    "light | a | point\n"
    "set | pos | -0.30, 0.20\n"
    "set | height | 0.30\n"
    "set | color | 1.0, 0.5, 0.3\n"
    "set | intensity | 2.5\n"
    "set | radius | 1.4\n"
    "light | b | point\n"
    "set | pos | 0.35, -0.10\n"
    "set | height | 0.25\n"
    "set | color | 0.3, 0.6, 1.0\n"
    "set | intensity | 2.0\n"
    "set | radius | 1.2\n";
const char* const THIRD_LIGHT =
    "light | c | point\n"
    "set | pos | 0.00, 0.40\n"
    "set | height | 0.20\n"
    "set | color | 0.4, 1.0, 0.5\n"
    "set | intensity | 1.6\n"
    "set | radius | 1.0\n";

} // namespace

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

int failures() { return fails; }

bool load_table(const std::string& src, std::vector<uint8_t>& bytes, light::Table& t,
                const char* what) {
    light::BakeError err;
    if (!light::bake_lights(src, bytes, err)) {
        std::printf("  FAIL: %s does not bake (line %d: %s)\n", what, err.line,
                    err.message.c_str());
        ++fails;
        return false;
    }
    const light::LoadResult lr = t.load(bytes.data(), bytes.size());
    if (lr != light::LoadResult::Ok) {
        std::printf("  FAIL: %s does not open (%s)\n", what, light::load_reason(lr));
        ++fails;
        return false;
    }
    return true;
}

void count_comes_from_data(GpuContext& gpu, matgold::Scene& scene, uint32_t w, uint32_t h) {
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    std::vector<uint8_t> b2, b3;
    light::Table t2, t3;
    if (!load_table(TWO_LIGHTS, b2, t2, "the two-light fixture")) return;
    if (!load_table(std::string(TWO_LIGHTS) + THIRD_LIGHT, b3, t3, "the three-light fixture"))
        return;

    lightgfx::Pass p2, p3;
    check(p2.init(gpu.device, gpu.queue, t2, WGPUTextureFormat_RGBA8Unorm, aspect),
          "a pass over two lights starts");
    check(p3.init(gpu.device, gpu.queue, t3, WGPUTextureFormat_RGBA8Unorm, aspect),
          "a pass over three lights starts");
    check(p2.lights() == 2 && p3.lights() == 3, "the pass takes its light count from the table");
    uint32_t d = 0;
    const std::vector<uint8_t> f2 = lightgold::render_frame(gpu, scene, &p2, w, h, d);
    const std::vector<uint8_t> f3 = lightgold::render_frame(gpu, scene, &p3, w, h, d);
    check(!f2.empty() && f2 != f3, "one more light in the table changes the frame");
    p3.shutdown();
    p2.shutdown();
}

void report_cost(GpuContext& gpu, matgold::Scene& scene, lightgfx::Pass& pass, uint32_t w,
                 uint32_t h) {
    uint32_t d = 0;
    const auto span = [&](lightgfx::Pass* p) {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < COST_FRAMES; ++i) lightgold::render_frame(gpu, scene, p, w, h, d);
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count() / COST_FRAMES;
    };
    const double off = span(nullptr);
    const double on = span(&pass);
    std::printf("  cost: %.3f ms/frame without the pass, %.3f ms/frame with it (%d frames each)\n",
                off, on, COST_FRAMES);
}

} // namespace lightgold
