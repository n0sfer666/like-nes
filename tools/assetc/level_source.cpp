#include "level_source.hpp"

#include "assetc_manifest.hpp"
#include "platform_fs.hpp"

namespace asset::manifest {
namespace {

constexpr size_t MAX_TILESET_BYTES = 16u << 20;

std::string stem_of(const std::string& path) {
    const size_t slash = path.rfind('/');
    const std::string file = slash == std::string::npos ? path : path.substr(slash + 1);
    return file.substr(0, file.rfind('.'));
}

} // namespace

bool LevelSource::join(const std::string& from, const std::string& rel, std::string& out,
                       std::string& error) const {
    if (rel.empty() || rel[0] == '/' || rel.find('\\') != std::string::npos || rel.find(':') != std::string::npos) {
        error = "path '" + rel + "' must be relative and use '/'";
        return false;
    }
    if (from.size() <= base_.size() + 1 || from.compare(0, base_.size(), base_) != 0 || from[base_.size()] != '/') {
        error = "file " + from + " lies outside the manifest directory " + base_;
        return false;
    }
    std::vector<std::string> segs;
    const std::string inside = from.substr(base_.size() + 1);
    const size_t rel_at = inside.rfind('/') + 1;
    const std::string joined = inside.substr(0, rel_at) + rel;
    size_t start = 0;
    while (start <= joined.size()) {
        const size_t slash = joined.find('/', start);
        const size_t end = slash == std::string::npos ? joined.size() : slash;
        const std::string seg = joined.substr(start, end - start);
        const bool in_rel = start >= rel_at;
        start = end + 1;
        if (in_rel && (seg.empty() || seg == ".")) {
            error = "path '" + rel + "' has an empty or '.' segment; write each directory once";
            return false;
        }
        if (seg != "..") {
            segs.push_back(seg);
            continue;
        }
        if (segs.empty()) {
            error = "path '" + rel + "' leaves the manifest directory";
            return false;
        }
        segs.pop_back();
    }
    out.clear();
    for (const std::string& s : segs) out += (out.empty() ? "" : "/") + s;
    return true;
}

bool LevelSource::tileset(const std::string& from, const std::string& rel, std::string& file,
                          std::vector<uint8_t>& bytes, std::string& error) {
    std::string norm;
    if (!join(from, rel, norm, error) || !resolve(base_, norm, file, error)) return false;
    deps_.push_back(file);
    if (platform::read_bytes_capped(file, bytes, MAX_TILESET_BYTES)) return true;
    error = "cannot read " + file + " (missing, unreadable or over 16 MiB)";
    return false;
}

bool LevelSource::image(const std::string& from, const std::string& rel, framework::tiled::Image& out,
                        std::string& error) {
    std::string norm;
    if (!join(from, rel, norm, error)) return false;
    const auto it = textures_.find(norm);
    if (it == textures_.end()) {
        error = "image '" + norm + "' is not a texture record; add a line such as texture|" + stem_of(norm) +
                "|pixel|" + norm;
        return false;
    }
    out = it->second;
    return true;
}

} // namespace asset::manifest
