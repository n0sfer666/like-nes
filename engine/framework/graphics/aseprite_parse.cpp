#include "aseprite_parts.hpp"
#include "clip_import.hpp"

namespace framework::graphics {
namespace {

std::string base_name(std::string_view path) {
    const std::size_t cut = path.find_last_of("/\\");
    return std::string(cut == std::string_view::npos ? path : path.substr(cut + 1));
}

bool sheet_of(const tiled::Doc& d, const json::Node& meta, tiled::Source& src, tiled::Image& out,
              std::string& error) {
    std::string_view image;
    if (!d.get(meta, "image", image, error, tiled::Need::Required)) return false;
    const std::string rel = base_name(image);
    if (rel.empty()) return d.fail(meta, "'image' names no file", error);
    std::string why;
    if (!src.image(d.file, rel, out, why)) return d.fail(meta, why, error);
    std::string_view scale = "1";
    if (!d.get(meta, "scale", scale, error)) return false;
    if (scale != "1") return d.fail(*d.field(meta, "scale"), "the sheet is scaled; export without --scale", error);
    clip_import::Box size;
    if (!clip_import::read_box(d, meta, "size", true, size, error)) return false;
    if (size.w != static_cast<int64_t>(out.width) || size.h != static_cast<int64_t>(out.height))
        return d.fail(meta, "'size' is " + std::to_string(size.w) + "x" + std::to_string(size.h) + " but " + rel +
                                " is " + std::to_string(out.width) + "x" + std::to_string(out.height) +
                                "; export the sheet and its JSON together",
                      error);
    return true;
}

} // namespace

bool import_aseprite(const std::string& record, const std::string& file, std::span<const std::byte> bytes,
                     tiled::Source& src, std::vector<ClipSrc>& out, std::string& error) {
    out.clear();
    tiled::Doc d;
    d.file = file;
    d.in = bytes;
    if (!d.load(error)) return false;
    const json::Node* meta = nullptr;
    if (!d.expect(d.root(), "meta", json::Kind::Object, tiled::Need::Required, meta, error)) return false;
    const json::Node* frames = d.field(d.root(), "frames");
    if (frames == nullptr) return d.fail(d.root(), "missing 'frames'", error);
    tiled::Image sheet;
    std::vector<clip_import::AseFrame> cells;
    std::vector<clip_import::AseTag> tags;
    std::vector<clip_import::AseSlice> slices;
    if (!sheet_of(d, *meta, src, sheet, error) || !clip_import::read_frames(d, *frames, sheet, cells, error) ||
        !clip_import::read_tags(d, *meta, cells.size(), tags, error) ||
        !clip_import::read_slices(d, *meta, cells.size(), slices, error))
        return false;
    for (const clip_import::AseTag& t : tags) {
        ClipSrc c;
        c.name = record + "/" + t.name;
        c.flags = t.flags;
        c.texture_guid = sheet.guid;
        if (!clip_import::build_clip(d, t, cells, slices, c.frames, error)) return false;
        out.push_back(std::move(c));
    }
    return true;
}

} // namespace framework::graphics
