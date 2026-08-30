#include "frame_golden.hpp"

#include <cmath>
#include <cstddef>

#include "stb_image.h"

namespace game {
namespace {

uint32_t largest_cluster(const std::vector<uint8_t>& mask, uint32_t w, uint32_t h) {
    std::vector<uint8_t> seen(mask.size(), 0);
    std::vector<uint32_t> stack;
    uint32_t best = 0;
    for (uint32_t start = 0; start < mask.size(); ++start) {
        if (!mask[start] || seen[start]) continue;
        stack.clear();
        stack.push_back(start);
        seen[start] = 1;
        uint32_t n = 0;
        while (!stack.empty()) {
            const uint32_t p = stack.back();
            stack.pop_back();
            ++n;
            const uint32_t x = p % w, y = p / w;
            const uint32_t nb[4] = {x > 0 ? p - 1 : p, x + 1 < w ? p + 1 : p,
                                    y > 0 ? p - w : p, y + 1 < h ? p + w : p};
            for (uint32_t q : nb) {
                if (q != p && mask[q] && !seen[q]) { seen[q] = 1; stack.push_back(q); }
            }
        }
        if (n > best) best = n;
    }
    return best;
}

} // namespace

FrameDiff compare_frames(const std::vector<uint8_t>& px, const std::vector<uint8_t>& ref,
                         uint32_t w, uint32_t h, FrameTolerance tol) {
    FrameDiff d;
    const std::size_t pixels = static_cast<std::size_t>(w) * h;
    if (px.empty() || px.size() != ref.size() || px.size() != pixels * 4u) return d;
    // Альфа в сравнение не входит: цель рендерится непрозрачной, и её канал одинаков по всему
    // кадру — включённый в среднюю ошибку, он развёл бы её на четверть и сделал допуск мягче в
    // ту же четверть, не сказав об этом.
    std::vector<uint8_t> mask(pixels, 0);
    double sum = 0.0;
    std::size_t over = 0;
    for (std::size_t i = 0; i < pixels; ++i) {
        double worst = 0.0;
        for (std::size_t c = 0; c < 3; ++c) {
            const double e = std::fabs(px[i * 4 + c] / 255.0 - ref[i * 4 + c] / 255.0);
            sum += e;
            if (e > worst) worst = e;
        }
        if (worst > d.max_abs) d.max_abs = worst;
        if (worst > tol.pixel_eps) { mask[i] = 1; ++over; }
    }
    d.mean_abs = sum / static_cast<double>(pixels * 3u);
    d.frac_over = static_cast<double>(over) / static_cast<double>(pixels);
    d.max_cluster = largest_cluster(mask, w, h);
    d.pass = d.frac_over <= tol.frac_tol && d.max_cluster <= tol.max_cluster;
    return d;
}

bool comparator_refuses_spoiled(const std::vector<uint8_t>& px, uint32_t w, uint32_t h) {
    const std::size_t pixels = static_cast<std::size_t>(w) * h;
    if (px.size() != pixels * 4u || w < 32 || h < 32) return false;

    std::vector<uint8_t> blot = px;
    for (uint32_t y = 0; y < 5; ++y)
        for (uint32_t x = 0; x < 5; ++x)
            for (uint32_t c = 0; c < 3; ++c) {
                uint8_t& v = blot[((y + 8) * w + x + 8) * 4 + c];
                v = static_cast<uint8_t>(255 - v);
            }

    std::vector<uint8_t> scatter = px;
    for (std::size_t i = 0; i < pixels; i += 400) {
        const int v = scatter[i * 4];
        scatter[i * 4] = static_cast<uint8_t>(v >= 128 ? v - 16 : v + 16);
    }

    std::vector<uint8_t> shifted(px.size(), 0);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 1; x < w; ++x)
            for (uint32_t c = 0; c < 4; ++c)
                shifted[(y * w + x) * 4 + c] = px[(y * w + x - 1) * 4 + c];

    return !compare_frames(blot, px, w, h, TOL_CROSS_BACKEND).pass &&
           !compare_frames(scatter, px, w, h, TOL_CROSS_BACKEND).pass &&
           !compare_frames(shifted, px, w, h, TOL_CROSS_BACKEND).pass;
}

bool read_png_rgba(const char* path, std::vector<uint8_t>& out, uint32_t& w, uint32_t& h) {
    int iw = 0, ih = 0, ch = 0;
    stbi_uc* p = stbi_load(path, &iw, &ih, &ch, 4);
    if (!p) return false;
    w = static_cast<uint32_t>(iw);
    h = static_cast<uint32_t>(ih);
    out.assign(p, p + static_cast<std::size_t>(iw) * static_cast<std::size_t>(ih) * 4u);
    stbi_image_free(p);
    return true;
}

} // namespace game
