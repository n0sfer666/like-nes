#include "codec.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <zstd.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace asset::codec {

namespace {

// Запуск внешнего пиннутого кодека (POSIX). Возврат true при exit-code 0.
bool run_tool(const std::vector<std::string>& argv) {
    std::vector<char*> c;
    for (const auto& s : argv) c.push_back(const_cast<char*>(s.c_str()));
    c.push_back(nullptr);
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // stdout/stderr кодека в /dev/null — не засоряем вывод assetc (кодек пишет в файл).
        if (std::freopen("/dev/null", "w", stdout)) { /* best-effort */ }
        if (std::freopen("/dev/null", "w", stderr)) { /* best-effort */ }
        execvp(c[0], c.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

} // namespace

std::vector<uint8_t> read_file(const std::string& path) {
    std::vector<uint8_t> data;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return data;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        data.resize(static_cast<size_t>(n));
        if (std::fread(data.data(), 1, data.size(), f) != data.size()) data.clear();
    }
    std::fclose(f);
    return data;
}

bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
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
    std::remove(tmp.c_str());
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
    std::remove(tmp_out.c_str());
    return !out.empty();
}

} // namespace asset::codec
