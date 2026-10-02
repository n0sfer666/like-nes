#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "bundle_lookup.hpp"
#include "bundle_writer.hpp"
#include "hash.hpp"
#include "platform_args.hpp"

using namespace asset;

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "[asset-lookup] FAIL: %s\n", what);
    ++fails;
}

uint64_t guid_of(const char* n) { return fnv1a(n, std::strlen(n)); }

AssetInput texture(const char* name, uint32_t w, uint32_t h, uint32_t bytes) {
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Texture; a.codec = Codec::Raw; a.residency = Residency::Stream;
    a.payload.assign(bytes, 0x7E);
    a.uncompressed_size = bytes;
    a.tex_w = w; a.tex_h = h; a.tex_format = TEX_FORMAT_RGBA8_UNORM;
    return a;
}

AssetInput table(const char* name, uint32_t bytes) {
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Raw; a.codec = Codec::Raw; a.residency = Residency::Mmap;
    a.payload.assign(bytes, 0x3C);
    a.uncompressed_size = bytes;
    return a;
}

std::vector<AssetInput> inputs() {
    std::vector<AssetInput> v;
    v.push_back(texture("good", 2, 3, 24));
    AssetInput zstd = texture("zstd", 2, 3, 24);
    zstd.codec = Codec::Zstd;
    v.push_back(zstd);
    AssetInput bgra = texture("bgra", 2, 3, 24);
    bgra.tex_format = TEX_FORMAT_RGBA8_UNORM + 5;
    v.push_back(bgra);
    v.push_back(texture("short", 2, 3, 20));
    v.push_back(texture("long", 2, 3, 28));
    v.push_back(texture("empty", 0, 3, 0));
    v.push_back(texture("flat", 2, 0, 0));
    AssetInput declared = texture("declared", 2, 3, 24);
    declared.uncompressed_size = 48;
    v.push_back(declared);
    v.push_back(table("visual", 40));
    AssetInput packed = table("packed", 40);
    packed.codec = Codec::Zstd;
    v.push_back(packed);
    return v;
}

alignas(64) uint8_t region[4096];

void textures(const BundleView& b) {
    RgbaView t;
    check(raw_rgba8(b, guid_of("good"), t) == LookupFault::Ok, "raw RGBA8 texture accepted");
    const AssetEntry* e = b.find(guid_of("good"));
    check(e != nullptr && t.pixels == b.payload(*e), "pixels point into the bundle region");
    check(t.width == 2 && t.height == 3, "texture sides from the entry");
    check(raw_rgba8(b, guid_of("zstd"), t) == LookupFault::WrongCodec, "compressed texture refused");
    check(t.pixels == nullptr && t.width == 0, "refusal clears the view");
    check(raw_rgba8(b, guid_of("bgra"), t) == LookupFault::WrongFormat, "foreign format refused");
    check(raw_rgba8(b, guid_of("short"), t) == LookupFault::WrongSize, "payload short of w*h*4");
    check(raw_rgba8(b, guid_of("long"), t) == LookupFault::WrongSize, "payload past w*h*4");
    check(raw_rgba8(b, guid_of("empty"), t) == LookupFault::WrongSize, "zero width refused");
    check(raw_rgba8(b, guid_of("flat"), t) == LookupFault::WrongSize, "zero height refused");
    check(raw_rgba8(b, guid_of("declared"), t) == LookupFault::WrongSize, "declared size differs");
    check(raw_rgba8(b, guid_of("visual"), t) == LookupFault::WrongType, "table is not a texture");
    check(raw_rgba8(b, guid_of("absent"), t) == LookupFault::Missing, "absent guid");
}

void tables(const BundleView& b) {
    const uint8_t* data = nullptr;
    size_t size = 0;
    check(raw_table(b, guid_of("visual"), data, size) == LookupFault::Ok, "raw table accepted");
    check(size == 40 && data != nullptr && data[0] == 0x3C && data[39] == 0x3C, "table bytes");
    check(raw_table(b, guid_of("packed"), data, size) == LookupFault::WrongCodec, "packed table");
    check(data == nullptr && size == 0, "refusal clears the table");
    check(raw_table(b, guid_of("good"), data, size) == LookupFault::WrongType, "texture as table");
    check(raw_table(b, guid_of("absent"), data, size) == LookupFault::Missing, "absent table");
    BundleView closed;
    check(raw_table(closed, guid_of("visual"), data, size) == LookupFault::Missing, "closed view");
}

void names() {
    const LookupFault all[] = {LookupFault::Ok, LookupFault::Missing, LookupFault::WrongType,
                               LookupFault::WrongCodec, LookupFault::WrongFormat,
                               LookupFault::WrongSize};
    for (LookupFault a : all)
        for (LookupFault b : all)
            if (a != b)
                check(std::strcmp(lookup_fault_name(a), lookup_fault_name(b)) != 0,
                      "fault names are distinct");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    const std::vector<uint8_t> bytes = write_bundle(inputs());
    if (bytes.size() > sizeof(region)) {
        std::fprintf(stderr, "[asset-lookup] FAIL: fixture bundle %zu bytes\n", bytes.size());
        return 1;
    }
    std::memcpy(region, bytes.data(), bytes.size());
    BundleView b;
    if (!b.open(region, bytes.size(), false)) {
        std::fprintf(stderr, "[asset-lookup] FAIL: fixture bundle does not open\n");
        return 1;
    }
    textures(b);
    tables(b);
    names();
    if (fails != 0) return 1;
    std::printf("asset-lookup: PASS\n");
    return 0;
}
