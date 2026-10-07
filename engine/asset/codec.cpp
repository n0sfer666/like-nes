#include "codec.hpp"

#include <climits>
#include <cstdio>
#include <cstring>
#include <zstd.h>

#include "platform_fs.hpp"
#include "platform_process.hpp"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace asset::codec {

namespace {

using platform::run_tool;

constexpr uint8_t PNG_SIGNATURE[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

std::string failure(const char* what) {
    const char* why = stbi_failure_reason();
    return std::string(what) + (why ? std::string(": ") + why : std::string());
}

} // namespace

std::vector<uint8_t> read_file(const std::string& path) {
    std::vector<uint8_t> data;
    platform::read_bytes(path, data);
    return data;
}

bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = platform::open_file(path, "wb");
    if (!f) return false;
    bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);
    return ok;
}

std::vector<uint8_t> zstd_compress(const std::vector<uint8_t>& in, int level) {
    size_t bound = ZSTD_compressBound(in.size());
    std::vector<uint8_t> out(bound);
    size_t n = ZSTD_compress(out.data(), bound, in.data(), in.size(), level);
    if (ZSTD_isError(n)) return {};
    out.resize(n);
    return out;
}

bool wgsl_to_spirv(const Tools& t, const std::string& wgsl_path, const std::string& ep,
                   std::vector<uint8_t>& out) {
    std::string tmp = wgsl_path + "." + ep + ".spv";
    if (!run_tool({t.tint, wgsl_path, "--format", "spirv", "--ep", ep, "-o", tmp})) return false;
    out = read_file(tmp);
    platform::remove_file(tmp);
    return !out.empty();
}

bool png_to_ktx2(const Tools& t, const std::string& png_path, const std::string& tmp_out,
                 std::vector<uint8_t>& out, uint32_t& w, uint32_t& h) {
    int iw = 0, ih = 0, comp = 0;
    if (!stbi_info(png_path.c_str(), &iw, &ih, &comp)) return false;
    w = static_cast<uint32_t>(iw);
    h = static_cast<uint32_t>(ih);
    // -no_multithreading = детерминированный байт-вывод (гейт #1). -ktx2_no_zstandard =
    // UASTC без zstd-суперкомпрессии → рантайм-транскодеру не нужен zstd (нет конфликта
    // символов с нашим libzstd); zstd-путь демонстрирует bulk-ассет.
    if (!run_tool({t.basisu, "-ktx2", "-uastc", "-ktx2_no_zstandard", "-no_multithreading",
                   "-file", png_path, "-output_file", tmp_out}))
        return false;
    out = read_file(tmp_out);
    platform::remove_file(tmp_out);
    return !out.empty();
}

// stb_image сам читает и JPEG, и BMP, и GIF: без сверки подписи путь pixel молча принял бы любой
// из них, а hd отдал бы его basisu, который их не ест.
bool png_info(const std::vector<uint8_t>& png, uint32_t& w, uint32_t& h, std::string& error) {
    if (png.size() < sizeof(PNG_SIGNATURE) ||
        std::memcmp(png.data(), PNG_SIGNATURE, sizeof(PNG_SIGNATURE)) != 0) {
        error = "not a PNG file";
        return false;
    }
    if (png.size() > static_cast<size_t>(INT_MAX)) {
        error = "PNG file larger than 2 GiB";
        return false;
    }
    const int len = static_cast<int>(png.size());
    int iw = 0, ih = 0, comp = 0;
    if (!stbi_info_from_memory(png.data(), len, &iw, &ih, &comp)) {
        error = failure("corrupt PNG");
        return false;
    }
    if (stbi_is_16_bit_from_memory(png.data(), len)) {
        error = "PNG has 16 bits per channel; export it with 8 bits per channel";
        return false;
    }
    w = static_cast<uint32_t>(iw);
    h = static_cast<uint32_t>(ih);
    if (w > MAX_TEXTURE_SIDE || h > MAX_TEXTURE_SIDE) {
        error = "PNG is " + std::to_string(w) + "x" + std::to_string(h) +
                ", a texture side is limited to " + std::to_string(MAX_TEXTURE_SIDE) + " px";
        return false;
    }
    return true;
}

bool png_to_rgba8(const std::vector<uint8_t>& png, std::vector<uint8_t>& rgba, uint32_t& w,
                  uint32_t& h, std::string& error) {
    if (!png_info(png, w, h, error)) return false;
    int iw = 0, ih = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &iw, &ih, &comp, 4);
    if (!px) {
        error = failure("corrupt PNG");
        return false;
    }
    // Размер буфера берётся из заголовка, прочитанного отдельно: расхождение двух чтений stb было
    // бы чтением за концом его буфера, а не ошибкой декодера.
    if (iw != static_cast<int>(w) || ih != static_cast<int>(h)) {
        stbi_image_free(px);
        error = "corrupt PNG: decoded size differs from the header";
        return false;
    }
    rgba.assign(px, px + static_cast<size_t>(w) * h * 4);
    stbi_image_free(px);
    return true;
}

} // namespace asset::codec
