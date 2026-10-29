#include "manifest_kind_bake.hpp"

#include "bakers.hpp"

namespace asset::manifest {

bool bake_kind(Bake& b, const TextRecord& f, std::span<const framework::graphics::ClipSrc> clips) {
    const Record& r = f.record;
    const Kind* kind = find_kind(b.kinds, r.kind);
    std::vector<uint8_t> table;
    KindError err;
    if (kind->bake(r.name.c_str(), f.text, clips, table, err)) {
        bakers::push_table(r.name.c_str(), std::move(table), b.assets);
        return true;
    }
    const std::string at = err.line > 0 ? ":" + std::to_string(err.line) : "";
    return bake_fail(b, r.line, f.file + at + ": " + err.message);
}

} // namespace asset::manifest
