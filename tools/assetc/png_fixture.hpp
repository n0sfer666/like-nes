#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// PNG для тестов пекаря, собранный руками: deflate «stored»-блоками, фильтр 0 у каждой строки.
// Свой кодировщик, а не stb_image_write: тот пишет только 8-битные RGB(A) и серый, а отказы
// пекаря живут как раз на палитре, 16 битах и заголовке с огромной стороной. Он же — оракул,
// независимый от декодера, который проверяется.
namespace png_fixture {

struct Image {
    uint32_t w = 1, h = 1;
    uint8_t depth = 8;
    uint8_t color = 6;              // 0 серый, 2 RGB, 3 палитра, 4 серый+альфа, 6 RGBA
    std::vector<uint8_t> palette;   // PLTE, по 3 байта
    std::vector<uint8_t> trns;      // tRNS как есть
    std::vector<uint8_t> rows;      // строки без байта фильтра; пусто — только заголовок
};

inline uint32_t crc32(const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFFu) {
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return c;
}

inline void be32(std::vector<uint8_t>& v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back(static_cast<uint8_t>(x >> s));
}

inline void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    be32(out, static_cast<uint32_t>(data.size()));
    std::vector<uint8_t> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    be32(out, crc32(body.data(), body.size()) ^ 0xFFFFFFFFu);
}

inline std::vector<uint8_t> zlib_stored(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> z{0x78, 0x01};
    size_t pos = 0;
    do {
        const size_t n = raw.size() - pos < 65535 ? raw.size() - pos : 65535;
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(static_cast<uint8_t>(n));
        z.push_back(static_cast<uint8_t>(n >> 8));
        z.push_back(static_cast<uint8_t>(~n));
        z.push_back(static_cast<uint8_t>(~n >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
    } while (pos < raw.size());
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) {
        a = (a + c) % 65521u;
        b = (b + a) % 65521u;
    }
    be32(z, (b << 16) | a);
    return z;
}

inline std::vector<uint8_t> encode(const Image& im) {
    std::vector<uint8_t> out{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    be32(ihdr, im.w);
    be32(ihdr, im.h);
    ihdr.insert(ihdr.end(), {im.depth, im.color, 0, 0, 0});
    chunk(out, "IHDR", ihdr);
    if (!im.palette.empty()) chunk(out, "PLTE", im.palette);
    if (!im.trns.empty()) chunk(out, "tRNS", im.trns);
    if (!im.rows.empty()) {
        const size_t stride = im.rows.size() / im.h;
        std::vector<uint8_t> raw;
        for (uint32_t y = 0; y < im.h; ++y) {
            raw.push_back(0);
            raw.insert(raw.end(), im.rows.begin() + static_cast<std::ptrdiff_t>(y * stride),
                       im.rows.begin() + static_cast<std::ptrdiff_t>((y + 1) * stride));
        }
        chunk(out, "IDAT", zlib_stored(raw));
    }
    chunk(out, "IEND", {});
    return out;
}

} // namespace png_fixture
