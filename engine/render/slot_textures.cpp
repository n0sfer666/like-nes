#include "slot_textures.hpp"

#include "../asset/hash.hpp"
#include "slot_encoding.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace slotgold {
namespace {

constexpr uint32_t TEX = 32;

// Круг спрайта из `material_textures.cpp`: за его пределами купола нет, там нормаль плоская, и
// перекрывать свет там тоже нечем.
constexpr float R2 = 0.2f;

uint8_t enc(float v) {
    const float t = (v * 0.5f + 0.5f) * 255.0f + 0.5f;
    return static_cast<uint8_t>(t < 0.0f ? 0.0f : (t > 255.0f ? 255.0f : t));
}

void put(uint8_t* p, float x, float y, float z) {
    const float len = std::sqrt(x * x + y * y + z * z);
    const float k = len > 1e-6f ? 1.0f / len : 0.0f;
    p[0] = enc(x * k);
    p[1] = enc(y * k);
    p[2] = enc(z * k);
    p[3] = 255;
}

void put_occ(uint8_t* p, float o) {
    const uint8_t v = static_cast<uint8_t>(o * 255.0f + 0.5f);
    p[0] = v; p[1] = v; p[2] = v; p[3] = 255;
}

WGPUTexture upload(WGPUDevice device, WGPUQueue queue, const uint8_t* px, uint32_t side) {
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{side, side, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    WGPUTexture tex = wgpuDeviceCreateTexture(device, &td);
    WGPUImageCopyTexture dst = {};
    dst.texture = tex;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = 4 * side;
    layout.rowsPerImage = side;
    WGPUExtent3D ext = {side, side, 1};
    wgpuQueueWriteTexture(queue, &dst, px, 4u * side * side, &layout, &ext);
    return tex;
}

WGPUTexture make_single(WGPUDevice device, WGPUQueue queue, const uint8_t rgba[4]) {
    return upload(device, queue, rgba, 1);
}

// Купол: полусфера, вписанная в круг спрайта. Нормаль полусферы радиуса r на расстоянии d от
// центра — (dx, dy, sqrt(r^2 - d^2)) / r; знак Y переворачивается ЗДЕСЬ, по контракту заголовка.
WGPUTexture make_dome(WGPUDevice device, WGPUQueue queue) {
    std::vector<uint8_t> px(4u * TEX * TEX);
    for (uint32_t y = 0; y < TEX; ++y) {
        for (uint32_t x = 0; x < TEX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / TEX - 0.5f;
            const float dy = (static_cast<float>(y) + 0.5f) / TEX - 0.5f;
            const float d2 = dx * dx + dy * dy;
            uint8_t* p = px.data() + 4 * (y * TEX + x);
            if (d2 >= R2) {
                put(p, 0.0f, 0.0f, 1.0f);
                continue;
            }
            put(p, dx, -dy, std::sqrt(R2 - d2));
        }
    }
    return upload(device, queue, px.data(), TEX);
}

// Вертикальные рёбра: рельеф меняется только по X, поэтому свет слева и свет справа дают РАЗНЫЙ
// кадр — купол на такое различие не отвечает, он симметричен.
WGPUTexture make_ridge(WGPUDevice device, WGPUQueue queue) {
    std::vector<uint8_t> px(4u * TEX * TEX);
    for (uint32_t y = 0; y < TEX; ++y) {
        for (uint32_t x = 0; x < TEX; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / TEX;
            const float nx = 0.7f * std::sin(6.2831853f * 4.0f * u);
            uint8_t* p = px.data() + 4 * (y * TEX + x);
            put(p, nx, 0.0f, std::sqrt(1.0f - nx * nx));
        }
    }
    return upload(device, queue, px.data(), TEX);
}

// Диск: перекрывает целиком внутри круга спрайта и не перекрывает вне его.
WGPUTexture make_disc(WGPUDevice device, WGPUQueue queue) {
    std::vector<uint8_t> px(4u * TEX * TEX);
    for (uint32_t y = 0; y < TEX; ++y) {
        for (uint32_t x = 0; x < TEX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / TEX - 0.5f;
            const float dy = (static_cast<float>(y) + 0.5f) / TEX - 0.5f;
            put_occ(px.data() + 4 * (y * TEX + x), dx * dx + dy * dy < R2 ? 1.0f : 0.0f);
        }
    }
    return upload(device, queue, px.data(), TEX);
}

// Решётка: вертикальные полосы, перекрывающие через одну. Форма выбрана так, чтобы ТЕНЬ от неё
// отличалась от тени диска не яркостью, а рисунком: одинаковые перекрыватели у двух материалов
// доказывали бы, что карта дошла до прохода, но не что она пришла из СВОЕЙ строки таблицы.
WGPUTexture make_grate(WGPUDevice device, WGPUQueue queue) {
    std::vector<uint8_t> px(4u * TEX * TEX);
    for (uint32_t y = 0; y < TEX; ++y) {
        for (uint32_t x = 0; x < TEX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / TEX - 0.5f;
            const float dy = (static_cast<float>(y) + 0.5f) / TEX - 0.5f;
            const bool bar = ((x * 4u) / TEX) % 2u == 0u;
            put_occ(px.data() + 4 * (y * TEX + x),
                    bar && dx * dx + dy * dy < R2 ? 1.0f : 0.0f);
        }
    }
    return upload(device, queue, px.data(), TEX);
}

uint64_t guid_of(const char* name) { return asset::fnv1a(name, std::strlen(name)); }

} // namespace

bool Bank::init(WGPUDevice device, WGPUQueue queue) {
    // Повторный старт без остановки удвоил бы банк и потерял бы текстуры первого набора: у карт
    // владелец — этот вектор, и ничей больше.
    shutdown();
    flat_ = make_single(device, queue, slotenc::FLAT_NORMAL);
    open_ = make_single(device, queue, slotenc::OPEN_OCC);
    flat_view_ = wgpuTextureCreateView(flat_, nullptr);
    open_view_ = wgpuTextureCreateView(open_, nullptr);

    const struct {
        const char* name;
        WGPUTexture (*make)(WGPUDevice, WGPUQueue);
    } wanted[] = {
        {"sprite_normal", make_dome},
        {"outline_normal", make_ridge},
        {"sprite_occluder", make_disc},
        {"grate_occluder", make_grate},
    };
    for (const auto& w : wanted) {
        Named n;
        n.name = w.name;
        n.guid = guid_of(w.name);
        n.tex = w.make(device, queue);
        n.view = wgpuTextureCreateView(n.tex, nullptr);
        // Текстура кладётся в банк ДО проверки вью: владельца у неё иначе нет, и на отказе
        // `shutdown()` её уже не найдёт.
        maps_.push_back(n);
        if (!n.view) return false;
    }
    return flat_view_ != nullptr && open_view_ != nullptr;
}

WGPUTextureView Bank::view(uint64_t guid) const {
    for (const Named& n : maps_)
        if (n.guid == guid) return n.view;
    return nullptr;
}

const char* Bank::name(uint64_t guid) const {
    for (const Named& n : maps_)
        if (n.guid == guid) return n.name;
    return nullptr;
}

void Bank::shutdown() {
    for (Named& n : maps_) {
        if (n.view) wgpuTextureViewRelease(n.view);
        if (n.tex) wgpuTextureRelease(n.tex);
    }
    maps_.clear();
    if (open_view_) wgpuTextureViewRelease(open_view_);
    if (flat_view_) wgpuTextureViewRelease(flat_view_);
    if (open_) wgpuTextureRelease(open_);
    if (flat_) wgpuTextureRelease(flat_);
    open_view_ = nullptr; flat_view_ = nullptr; open_ = nullptr; flat_ = nullptr;
}

} // namespace slotgold
