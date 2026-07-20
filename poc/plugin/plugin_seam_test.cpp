#include "registry.hpp"
#include "builtin.hpp"
#include "host.hpp"
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: plugin_seam_test <gravity.so> <rle_codec.so> <multi.so>\n");
        return 2;
    }
    const std::string g = argv[1], rle = argv[2], multi = argv[3];

    Registry reg;
    PluginHost host(reg);
    reg.set_current_owner("builtin");
    reg.add_ecs_system("integrate", {"gravity"}, sys_integrate);
    reg.set_current_owner("");

    bool loaded = host.load_native(g) && host.load_native(rle) && host.load_native(multi);

    AssetDecodeFn dec = reg.find_codec("RLE0");
    std::vector<uint8_t> encoded = {4, 0xAB, 2, 0xCD, 3, 0x01};
    std::vector<uint8_t> out(64, 0);
    int32_t n = dec ? dec(encoded.data(), (int32_t)encoded.size(), out.data(), (int32_t)out.size()) : -1;
    std::vector<uint8_t> expect = {0xAB, 0xAB, 0xAB, 0xAB, 0xCD, 0xCD, 0x01, 0x01, 0x01};
    bool codec_ok = (n == (int32_t)expect.size());
    for (int32_t i = 0; codec_ok && i < n; ++i) codec_ok = (out[i] == expect[i]);

    size_t c_ecs = reg.count(EXT_ECS_SYSTEM);
    size_t c_codec = reg.count(EXT_ASSET_CODEC);
    size_t c_render = reg.count(EXT_RENDER_PASS);
    size_t c_input = reg.count(EXT_INPUT_SOURCE);
    size_t c_audio = reg.count(EXT_AUDIO_BUS);
    size_t c_ui = reg.count(EXT_UI_PANEL);

    std::printf("[plugin-seam] loaded plugins:        %s\n", loaded ? "YES" : "NO");
    std::printf("[plugin-seam] asset-codec RLE0 decode: %s (n=%d)\n", codec_ok ? "OK" : "BAD", n);
    std::printf("[plugin-seam] ext-points: ecs=%zu codec=%zu render=%zu input=%zu audio=%zu ui=%zu\n",
                c_ecs, c_codec, c_render, c_input, c_audio, c_ui);

    bool all_kinds = c_ecs >= 1 && c_codec >= 1 && c_render >= 1 && c_input >= 1 && c_audio >= 1 && c_ui >= 1;
    bool pass = loaded && codec_ok && all_kinds;
    std::printf("plugin-seam: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
