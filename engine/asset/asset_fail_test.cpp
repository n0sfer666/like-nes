#include <chrono>
#include <thread>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "asset_manager.hpp"
#include "bundle_writer.hpp"
#include "codec.hpp"
#include "hash.hpp"
#include "platform_args.hpp"

using namespace asset;

namespace {

uint64_t guid_of(const char* n) { return fnv1a(n, std::strlen(n)); }

AssetInput stream_asset(const char* name, uint32_t size, uint32_t declared) {
    std::vector<uint8_t> bytes(size, 0x5A);
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Bulk; a.codec = Codec::Zstd; a.residency = Residency::Stream;
    a.uncompressed_size = declared;
    a.payload = codec::zstd_compress(bytes, 19);
    return a;
}

std::vector<uint8_t> make_bundle(uint32_t bad_declared) {
    return write_bundle({stream_asset("bad", 8192, bad_declared), stream_asset("probe", 256, 256),
                         stream_asset("probe2", 256, 256)});
}

int fail(const char* m) { std::fprintf(stderr, "[asset-fail] FAIL: %s\n", m); return 1; }

bool settle(AssetManager& am, uint64_t g) {
    for (int i = 0; i < 2500; ++i) {
        am.sync_point();
        if (am.is_ready(g) || am.is_failed(g)) return true;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    return false;
}

const char* fails_then_drains(AssetManager& am, uint64_t marker) {
    const uint64_t bad = guid_of("bad");
    am.request(bad);
    am.request(marker);
    if (!settle(am, marker) || !am.is_ready(marker)) return "marker not ready";
    if (!am.is_failed(bad) || am.is_ready(bad)) return "failed load is not reported as failed";
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const std::string dir = argc >= 2 ? argv[1] : ".";
    const std::string lying = dir + "/af_lying.bundle", honest = dir + "/af_honest.bundle";
    const std::string missing = dir + "/af_missing.bundle";
    if (!codec::write_file(lying, make_bundle(8192 + 16))) return fail("write lying");
    if (!codec::write_file(honest, make_bundle(8192))) return fail("write honest");
    if (!codec::write_file(missing, write_bundle({stream_asset("probe", 256, 256)})))
        return fail("write missing");
    const uint64_t bad = guid_of("bad");

    AssetManager small;
    if (!small.open(honest, 4096, /*trusted=*/false)) return fail("open small arena");
    if (const char* m = fails_then_drains(small, guid_of("probe"))) return fail(m);

    AssetManager am;
    if (!am.open(lying, 1u << 20, /*trusted=*/false)) return fail("open lying");
    if (const char* m = fails_then_drains(am, guid_of("probe"))) return fail(m);
    am.release(bad);
    am.request(bad);
    am.request(guid_of("probe2"));
    if (!settle(am, guid_of("probe2"))) return fail("probe2 not ready");
    if (am.arena_allocations() != 3) return fail("failed asset was loaded again");
    if (!am.is_failed(bad)) return fail("failed state lost on re-request");

    if (!am.reload(honest)) return fail("reload honest");
    if (!settle(am, bad)) return fail("reloaded asset neither ready nor failed");
    if (!am.is_ready(bad) || am.is_failed(bad)) return fail("reload did not clear the failed state");

    if (!am.reload(missing)) return fail("reload missing");
    if (!settle(am, guid_of("probe"))) return fail("probe not ready after reload missing");
    if (am.view().find(bad) || am.is_ready(bad)) return fail("vanished asset still visible");
    if (am.is_failed(bad)) return fail("absence is reported as a load failure");

    std::printf("asset-fail: PASS\n");
    return 0;
}
