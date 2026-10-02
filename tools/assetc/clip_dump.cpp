#include <cstdio>
#include <string>
#include <vector>

#include "baker_guid.hpp"
#include "bundle_view.hpp"
#include "clip_read.hpp"
#include "platform_args.hpp"
#include "platform_fs.hpp"

namespace {

using namespace framework::graphics;

std::string box_name(const ClipBox& b) {
    if (b.kind == static_cast<uint8_t>(BoxKind::Push)) return "push";
    return (b.kind == static_cast<uint8_t>(BoxKind::Hit) ? "hit" : "hurt") + std::to_string(b.index);
}

void dump(const ClipView& c, const char* name) {
    for (uint32_t i = 0; i < c.clip.frame_count; ++i) {
        std::string boxes;
        for (const ClipBox& b : cel_boxes(c, c.cels[i])) boxes += (boxes.empty() ? "" : ",") + box_name(b);
        const char* event = clip_event_name(c, c.clip.frames[i].event);
        std::printf("%s frame=%u boxes=%s event=%s\n", name, i, boxes.empty() ? "-" : boxes.c_str(),
                    *event == '\0' ? "-" : event);
    }
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc != 2) {
        std::fprintf(stderr, "usage: clip_dump <bundle>\n");
        return 2;
    }
    std::vector<uint8_t> raw;
    static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= asset::BASE_ALIGN,
                  "a heap buffer must satisfy the bundle base alignment");
    asset::BundleView view;
    if (!platform::read_bytes(argv[1], raw) || !view.open(raw.data(), raw.size(), false)) {
        std::fprintf(stderr, "clip_dump: %s is not a readable bundle\n", argv[1]);
        return 1;
    }
    const asset::AssetEntry* e = view.find(asset::bakers::guid_of("clips"));
    ClipTable table;
    if (e == nullptr || e->payload_size != e->uncompressed_size || !table.open(view.payload(*e), e->payload_size)) {
        std::fprintf(stderr, "clip_dump: %s has no readable clips section\n", argv[1]);
        return 1;
    }
    for (uint32_t i = 0; i < table.count(); ++i) dump(table.clip(i), table.name(i));
    return 0;
}
