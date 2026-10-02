#include "bundle_lookup.hpp"

namespace asset {
namespace {

LookupFault raw_entry(const BundleView& bundle, uint64_t guid, AssetType type, const AssetEntry*& e) {
    e = bundle.valid() ? bundle.find(guid) : nullptr;
    if (e == nullptr) return LookupFault::Missing;
    if (e->type != static_cast<uint32_t>(type)) return LookupFault::WrongType;
    if (e->codec != static_cast<uint32_t>(Codec::Raw)) return LookupFault::WrongCodec;
    if (e->uncompressed_size != e->payload_size) return LookupFault::WrongSize;
    return LookupFault::Ok;
}

} // namespace

const char* lookup_fault_name(LookupFault f) {
    switch (f) {
    case LookupFault::Ok: return "ok";
    case LookupFault::Missing: return "missing";
    case LookupFault::WrongType: return "wrong type";
    case LookupFault::WrongCodec: return "wrong codec";
    case LookupFault::WrongFormat: return "wrong texture format";
    case LookupFault::WrongSize: return "wrong size";
    }
    return "unknown";
}

LookupFault raw_rgba8(const BundleView& bundle, uint64_t guid, RgbaView& out) {
    out = RgbaView{};
    const AssetEntry* e = nullptr;
    const LookupFault f = raw_entry(bundle, guid, AssetType::Texture, e);
    if (f != LookupFault::Ok) return f;
    if (e->tex_format != TEX_FORMAT_RGBA8_UNORM) return LookupFault::WrongFormat;
    if (e->tex_w == 0 || e->tex_h == 0 || uint64_t{e->tex_w} * e->tex_h * 4 != e->payload_size)
        return LookupFault::WrongSize;
    out = {bundle.payload(*e), e->tex_w, e->tex_h};
    return LookupFault::Ok;
}

LookupFault raw_table(const BundleView& bundle, uint64_t guid, const uint8_t*& data, size_t& size) {
    data = nullptr;
    size = 0;
    const AssetEntry* e = nullptr;
    const LookupFault f = raw_entry(bundle, guid, AssetType::Raw, e);
    if (f != LookupFault::Ok) return f;
    data = bundle.payload(*e);
    size = e->payload_size;
    return LookupFault::Ok;
}

} // namespace asset
