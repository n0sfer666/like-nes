#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

#include "bundle_writer.hpp"
#include "hash.hpp"
#include "viewport_load.hpp"

namespace {
namespace gr = framework::graphics;
namespace sc = framework::scene;
int fails = 0;
void check(bool ok, const char* why) { if (!ok) { ++fails; std::printf("  FAIL: %s\n", why); } }
constexpr gr::ViewportSettings CUSTOM{{640, 360}, {960, 540}};

bool same(const gr::ViewportSettings& a, const gr::ViewportSettings& b) {
    return a.zone.w == b.zone.w && a.zone.h == b.zone.h && a.limit.w == b.limit.w && a.limit.h == b.limit.h;
}

using Reader = bool (*)(const void*, std::size_t, gr::ViewportSettings&);
bool roundtrip(Reader read, const std::vector<uint8_t>& bytes) {
    gr::ViewportSettings out;
    return read(bytes.data(), bytes.size(), out) && same(out, CUSTOM);
}
bool ignores(const void*, std::size_t, gr::ViewportSettings& out) { out = {}; return true; }

bool bundle(std::vector<uint8_t> payload, bool present, asset::AssetType type, asset::BundleView& out) {
    const uint32_t size = static_cast<uint32_t>(payload.size());
    std::vector<asset::AssetInput> assets;
    if (present) assets.push_back({asset::fnv1a_str("viewport"), type, asset::Codec::Raw,
        asset::Residency::Mmap, std::move(payload), size});
    const auto bytes = asset::write_bundle(std::move(assets));
    alignas(64) static std::array<uint8_t, 4096> storage;
    if (bytes.size() > storage.size()) return false;
    std::memcpy(storage.data(), bytes.data(), bytes.size());
    return out.open(storage.data(), bytes.size(), false);
}

} // namespace

int main() {
    std::vector<uint8_t> bytes;
    std::string error;
    check(gr::bake_viewport_settings(CUSTOM, bytes, error), "custom settings bake");
    check(roundtrip(gr::read_viewport_settings, bytes) && !roundtrip(ignores, bytes),
          "control: ignoring the stored dimensions breaks roundtrip");
    gr::ViewportSettings out = CUSTOM;
    for (std::size_t size = 0; size < bytes.size(); ++size)
        check(!gr::read_viewport_settings(bytes.data(), size, out) && gr::viewport_settings_default(out),
              "every truncated section is refused and resets stale output");
    auto bad = bytes;
    bad[0] ^= 1;
    check(!gr::read_viewport_settings(bad.data(), bad.size(), out), "foreign magic is refused");
    bad = bytes;
    const uint32_t zero = 0;
    std::memcpy(bad.data() + 24, &zero, sizeof zero);
    check(!gr::read_viewport_settings(bad.data(), bad.size(), out), "a zero side is refused by the reader");
    std::vector<uint8_t> unaligned(bytes.size() + 1);
    std::memcpy(unaligned.data() + 1, bytes.data(), bytes.size());
    check(!gr::read_viewport_settings(unaligned.data() + 1, bytes.size(), out), "unaligned input is refused");
    auto invalid = CUSTOM;
    invalid.limit.h = gr::VIEWPORT_SIDE_MAX + 1;
    check(!gr::bake_viewport_settings(invalid, bad, error) && bad.empty() && !error.empty(), "out-of-range bake resets output");
    asset::BundleView view;
    check(bundle({}, false, asset::AssetType::Raw, view), "a bundle without settings opens");
    out = CUSTOM;
    check(sc::load_viewport_settings(view, out, error) && gr::viewport_settings_default(out) && error.empty(),
          "missing optional section means defaults");
    check(bundle(bytes, true, asset::AssetType::Raw, view) && sc::load_viewport_settings(view, out, error) && same(out, CUSTOM),
          "explicit SDK loading reads the optional section");
    check(bundle(bytes, true, asset::AssetType::Shader, view) && !sc::load_viewport_settings(view, out, error) &&
          gr::viewport_settings_default(out) && error.find("viewport table:") == 0, "wrong asset type is refused");
    bytes[0] ^= 1;
    check(bundle(bytes, true, asset::AssetType::Raw, view) && !sc::load_viewport_settings(view, out, error) &&
          error == "viewport table does not open", "present corruption never falls back silently");
    check(!sc::load_viewport_settings({}, out, error), "an unopened bundle is not a missing optional section");
    std::printf("framework-viewport-settings: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
