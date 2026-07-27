#include "codec.hpp"

#include <cstdio>
#include <zstd.h>

#include "platform_fs.hpp"
#include "platform_process.hpp"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace asset::codec {

namespace {

using platform::run_tool;

} // namespace

std::vector<uint8_t> read_file(const std::string& path) {
    std::vector<uint8_t> data;
    platform::read_bytes(path, data);
    return data;
}

bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = platform::open_file(path, "wb");
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
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

} // namespace asset::codec
