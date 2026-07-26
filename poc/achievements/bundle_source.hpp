#pragma once
#include <memory>
#include <string>

#include "manifest.hpp"

namespace asset {
class AssetManager;
}

namespace ach {

enum class SourceResult : uint32_t {
    Ok = 0,
    NoBundle = 1,
    NoAsset = 2,
    BadManifest = 3,
    AlreadyOpen = 4,
};

struct SourceStatus {
    SourceResult result = SourceResult::Ok;
    LoadResult load = LoadResult::Ok;
};

// Держит бандл замапленным: строки реестра указывают внутрь mmap-региона (zero-copy).
// Разрушение источника делает эти указатели висячими — источник обязан пережить Registry.
class BundleSource {
public:
    BundleSource();
    ~BundleSource();
    BundleSource(const BundleSource&) = delete;
    BundleSource& operator=(const BundleSource&) = delete;

    SourceStatus open(Registry& reg, const std::string& bundle_path);
    void close();
    bool is_open() const { return open_; }

private:
    std::unique_ptr<asset::AssetManager> am_;
    bool open_ = false;
};

const char* source_reason(SourceStatus s);

} // namespace ach
