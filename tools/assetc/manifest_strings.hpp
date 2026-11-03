#pragma once
#include "assetc_manifest.hpp"
#include "manifest_bake.hpp"
#include "font_bake.hpp"

namespace asset::manifest {
struct StringsRecord {
    Record record;
    std::string file;
};
bool bake_strings(Bake& bake, const StringsRecord& source, std::span<const framework::graphics::FontSrc> fonts);
}
