#include "frame_golden.hpp"

#include <cmath>

#include "stb_image.h"

namespace game {

FrameDiff compare_frames(const std::vector<uint8_t>& px, const std::vector<uint8_t>& ref,
                         FrameTolerance tol) {
    FrameDiff d;
    if (px.empty() || px.size() != ref.size()) return d;
    double sum = 0.0;
    std::size_t over = 0;
    // Альфа в сравнение не входит: цель рендерится непрозрачной, и её канал одинаков по всему
    // кадру — включённый в среднюю ошибку, он развёл бы её на четверть и сделал допуск мягче в
    // ту же четверть, не сказав об этом.
    std::size_t channels = 0;
    for (std::size_t i = 0; i < px.size(); i += 4) {
        for (std::size_t c = 0; c < 3; ++c) {
            const double a = px[i + c] / 255.0, b = ref[i + c] / 255.0;
            const double e = std::fabs(a - b);
            sum += e;
            if (e > d.max_abs) d.max_abs = e;
            if (e > tol.pixel_eps) ++over;
            ++channels;
        }
    }
    if (channels == 0) return d;
    d.mean_abs = sum / static_cast<double>(channels);
    d.frac_over = static_cast<double>(over) / static_cast<double>(channels);
    d.pass = d.frac_over <= tol.frac_tol && d.max_abs <= tol.max_cap;
    return d;
}

bool comparator_refuses_spoiled(const std::vector<uint8_t>& px) {
    if (px.size() < 24u * 24u * 4u) return false;
    std::vector<uint8_t> blot = px;
    for (std::size_t i = 0; i < 24u * 24u * 4u; ++i) blot[i] = static_cast<uint8_t>(255 - blot[i]);
    std::vector<uint8_t> drift = px;
    for (std::size_t i = 0; i < drift.size(); ++i) {
        const int v = drift[i];
        drift[i] = static_cast<uint8_t>(v >= 128 ? v - 8 : v + 8);
    }
    return !compare_frames(blot, px, TOL_CROSS_BACKEND).pass &&
           !compare_frames(drift, px, TOL_CROSS_BACKEND).pass;
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
