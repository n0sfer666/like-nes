#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "asset_manager.hpp"
#include "bundle_writer.hpp"
#include "codec.hpp"
#include "hash.hpp"

// Hot-reload roundtrip (спека #5 gate): source change → rebake → runtime свап ассета.
// Стабильный guid переживает пересборку; refcount'ы держатся; содержимое обновляется.
// Self-contained (writer генерирует v1/v2), без tint/basisu/GPU → CI на всех POSIX.

using namespace asset;

namespace {

uint64_t guid_of(const char* n) { return fnv1a(n, std::strlen(n)); }

// Бандл с 2 ассетами (shader Mmap zero-copy + bulk Stream/zstd), стабильные guid, вариантные байты.
std::vector<uint8_t> make_bundle(uint8_t shader_fill, uint8_t bulk_fill) {
    std::vector<uint8_t> shader(256, shader_fill);
    std::vector<uint8_t> bulk(8192, bulk_fill);

    AssetInput sh;
    sh.guid = guid_of("sprite.vs");
    sh.type = AssetType::Shader; sh.codec = Codec::SpirV; sh.residency = Residency::Mmap;
    sh.payload = shader; sh.uncompressed_size = static_cast<uint32_t>(shader.size());

    AssetInput bk;
    bk.guid = guid_of("scene_bulk");
    bk.type = AssetType::Bulk; bk.codec = Codec::Zstd; bk.residency = Residency::Stream;
    bk.uncompressed_size = static_cast<uint32_t>(bulk.size());
    bk.payload = codec::zstd_compress(bulk, 19);

    return write_bundle({sh, bk});
}

int fail(const char* m) { std::fprintf(stderr, "[hot-reload] FAIL: %s\n", m); return 1; }

void pump(AssetManager& am, uint64_t a, uint64_t b) {
    for (int i = 0; i < 500; ++i) {
        am.sync_point();
        if (am.is_ready(a) && am.is_ready(b)) return;
        usleep(200);
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc >= 2 ? argv[1] : ".";
    const std::string v1 = dir + "/hr_v1.bundle", v2 = dir + "/hr_v2.bundle";
    if (!codec::write_file(v1, make_bundle(0xAA, 0x11))) return fail("write v1");
    if (!codec::write_file(v2, make_bundle(0xBB, 0x22))) return fail("write v2");

    const uint64_t g_sh = guid_of("sprite.vs"), g_bk = guid_of("scene_bulk");

    AssetManager am;
    if (!am.open(v1, 1u << 20, /*trusted=*/false)) return fail("open v1");
    am.request(g_sh); am.request(g_bk);
    pump(am, g_sh, g_bk);
    if (!am.is_ready(g_sh) || !am.is_ready(g_bk)) return fail("v1 not ready");

    Loaded sh1 = am.get(g_sh), bk1 = am.get(g_bk);
    if (!sh1.data || !bk1.data) return fail("v1 null loaded");
    if (!sh1.zero_copy || sh1.data[0] != 0xAA) return fail("v1 shader content");
    if (bk1.data[0] != 0x11 || bk1.size != 8192) return fail("v1 bulk content");
    const uint64_t sh1h = fnv1a(sh1.data, sh1.size), bk1h = fnv1a(bk1.data, bk1.size);

    // --- rebake случился: свап на v2, guid'ы стабильны, refcount'ы держатся ---
    if (!am.reload(v2)) return fail("reload v2");
    pump(am, g_sh, g_bk);
    if (!am.is_ready(g_sh) || !am.is_ready(g_bk)) return fail("v2 not ready");

    Loaded sh2 = am.get(g_sh), bk2 = am.get(g_bk);
    if (!sh2.data || !bk2.data) return fail("v2 null loaded");
    if (!sh2.zero_copy || sh2.data[0] != 0xBB) return fail("v2 shader not swapped");
    if (bk2.data[0] != 0x22 || bk2.size != 8192) return fail("v2 bulk not swapped");
    const uint64_t sh2h = fnv1a(sh2.data, sh2.size), bk2h = fnv1a(bk2.data, bk2.size);

    if (sh1h == sh2h || bk1h == bk2h) return fail("content did not change on reload");

    // --- Негатив (регрессия critical-фикса): reload битого/отсутствующего бандла → false,
    // старое состояние ЖИВО (транзакционный reload не делает munmap до валидации) ---
    if (am.reload(dir + "/does_not_exist.bundle")) return fail("reload of missing must fail");
    pump(am, g_sh, g_bk);
    Loaded sh3 = am.get(g_sh), bk3 = am.get(g_bk);
    if (!sh3.data || sh3.data[0] != 0xBB) return fail("shader lost after failed reload (UAF)");
    if (!bk3.data || bk3.data[0] != 0x22) return fail("bulk lost after failed reload (UAF)");

    std::printf("[hot-reload] PASS roundtrip: shader %02x→%02x zero-copy, bulk %02x→%02x zstd; "
                "guid стабилен, содержимое обновлено; битый reload → old state жив\n",
                0xAA, 0xBB, 0x11, 0x22);
    am.close();
    return 0;
}
