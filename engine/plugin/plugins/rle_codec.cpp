#include "../plugin_api.h"

PLUGIN_EXPORT_ABI

extern "C" int32_t rle_decode(const uint8_t* in, int32_t n, uint8_t* out, int32_t cap) {
    int32_t o = 0;
    for (int32_t i = 0; i + 1 < n; i += 2) {
        int32_t count = in[i];
        uint8_t val = in[i + 1];
        for (int32_t k = 0; k < count; ++k) {
            if (o >= cap) return -1;
            out[o++] = val;
        }
    }
    return o;
}

extern "C" PLATFORM_EXPORT void plugin_main(const HostApi* h) {
    h->register_asset_codec(h->ctx, "RLE0", rle_decode);
    h->log(h->ctx, "RLE0 codec registered");
}
