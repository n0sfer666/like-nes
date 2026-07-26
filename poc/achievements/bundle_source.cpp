#include "bundle_source.hpp"

#include "asset_manager.hpp"

namespace ach {
namespace {

constexpr std::size_t ARENA_CAPACITY = 64u * 1024u;

} // namespace

BundleSource::BundleSource() : am_(new asset::AssetManager()) {}

BundleSource::~BundleSource() { close(); }

// Один источник — один маппинг: замена бандла под живым Registry оставила бы уже принятые строки
// указывать в размапленный регион. Реестр своих доноров не знает, поэтому отказ — на входе.
SourceStatus BundleSource::open(Registry& reg, const std::string& bundle_path) {
    SourceStatus st;
    if (open_) {
        st.result = SourceResult::AlreadyOpen;
        return st;
    }
    if (!am_->open(bundle_path, ARENA_CAPACITY, /*trusted=*/false)) {
        st.result = SourceResult::NoBundle;
        return st;
    }
    open_ = true;
    am_->request(MANIFEST_ASSET_GUID);
    am_->sync_point();
    if (!am_->is_ready(MANIFEST_ASSET_GUID)) {
        close();
        st.result = SourceResult::NoAsset;
        return st;
    }
    const asset::Loaded a = am_->get(MANIFEST_ASSET_GUID);
    st.load = load_manifest(reg, a.data, a.size);
    if (st.load != LoadResult::Ok) {
        close();
        st.result = SourceResult::BadManifest;
    }
    return st;
}

void BundleSource::close() {
    if (open_) {
        am_->close();
        open_ = false;
    }
}

const char* source_reason(SourceStatus s) {
    switch (s.result) {
        case SourceResult::Ok: return "ok";
        case SourceResult::NoBundle: return "bundle not readable";
        case SourceResult::NoAsset: return "no achievements asset in bundle";
        case SourceResult::BadManifest: return load_reason(s.load);
        case SourceResult::AlreadyOpen: return "source already holds a bundle";
    }
    return "unknown";
}

} // namespace ach
