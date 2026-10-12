#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "bundle_writer.hpp"
#include "fighter_bake.hpp"
#include "framework_brawl_fighter_fixture.hpp"
#include "framework_scene_fixture.hpp"
#include "hash.hpp"
#include "level_load.hpp"
#include "map_bake.hpp"
#include "visual_bake.hpp"

namespace level_fixture {

namespace tl = framework::tilemap;
namespace gr = framework::graphics;

constexpr uint64_t STREET = asset::fnv1a_str("street_tiles");
constexpr uint64_t SKY = asset::fnv1a_str("sky");
constexpr uint64_t SHEET = asset::fnv1a_str("fighter_sheet");
constexpr std::size_t REGION = 256u * 1024u;

alignas(64) inline uint8_t region[REGION];

struct Source {
    std::vector<tl::ParsedMap> maps;
    std::vector<tl::VisualMapSrc> visuals;
    std::vector<tl::ObjectMapSrc> objects;
    std::vector<gr::ClipSrc> clips;
    std::vector<std::string> fighters;
    std::vector<uint64_t> images;
};

inline tl::ParsedMap grid(const char* name, uint32_t w, uint32_t h) {
    tl::ParsedMap m;
    m.name = name;
    m.tile_size = fix32::from_int(16);
    m.width = w;
    m.height = h;
    m.flags.assign(static_cast<std::size_t>(w) * h, 0);
    return m;
}

inline tl::VisualMapSrc visual(const char* name, uint32_t w, uint32_t h, bool sky) {
    tl::VisualMapSrc v;
    v.name = name;
    v.width = w;
    v.height = h;
    v.tile_size = 16;
    v.tilesets = {tl::VisualTileset{STREET, 1, 4, 2, 0, 0, 0}};
    tl::VisualLayerSrc ground;
    ground.name = "ground";
    ground.cells.assign(static_cast<std::size_t>(w) * h, 1);
    v.layers = {ground};
    if (!sky) return v;
    tl::VisualLayerSrc back;
    back.name = "sky";
    back.kind = tl::LayerKind::Image;
    back.image_guid = SKY;
    back.image_w = 64;
    back.image_h = 32;
    v.layers.insert(v.layers.begin(), back);
    return v;
}

inline std::vector<gr::ClipSrc> sheet_clips(const std::string& sheet) {
    std::vector<gr::ClipSrc> v = framework::brawl::test::fixture_clips();
    for (gr::ClipSrc& c : v) {
        c.name = sheet + c.name.substr(c.name.find('/'));
        c.texture_guid = SHEET;
        for (gr::ClipFrameSrc& f : c.frames) f.w = f.h = 16;
    }
    return v;
}

inline std::string fighter_text(const std::string& sheet) {
    std::string t = framework::brawl::test::FIGHTER_TEXT;
    const std::string from = "| banderas";
    t.replace(t.find(from), from.size(), "| " + sheet);
    return t;
}

inline void add_fighter(Source& s, const std::string& kind) {
    const std::vector<gr::ClipSrc> c = sheet_clips(kind);
    s.clips.insert(s.clips.end(), c.begin(), c.end());
    s.fighters.push_back(kind);
}

inline void add_kinds(Source& s, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        const std::string kind = "kind" + std::to_string(i);
        s.objects[0].objects.push_back(
            scene_fixture::wave_point(100 + i, ("extra" + kind).c_str(), 400, 250, 1, 0, kind.c_str()));
        add_fighter(s, kind);
    }
}

inline void add_image(Source& s, const std::string& name, uint64_t guid) {
    tl::VisualLayerSrc l;
    l.name = name;
    l.kind = tl::LayerKind::Image;
    l.image_guid = guid;
    l.image_w = 1;
    l.image_h = 1;
    s.visuals[0].layers.push_back(l);
}

inline void add_images(Source& s, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        const std::string name = "image" + std::to_string(i);
        add_image(s, name, asset::fnv1a_str(name.c_str()));
        s.images.push_back(asset::fnv1a_str(name.c_str()));
    }
}

inline Source source() {
    Source s;
    s.maps = {grid("lvl", 4, 2), grid("two", 3, 3)};
    s.visuals = {visual("lvl", 4, 2, true), visual("two", 3, 3, false)};
    s.objects = {scene_fixture::level(), {"two", {scene_fixture::point(1, "player", "spawn", 8, 8)}}};
    for (const char* f : {"adler", "banderas"}) add_fighter(s, f);
    return s;
}

inline asset::AssetInput table(const char* name, std::vector<uint8_t> bytes) {
    asset::AssetInput a;
    a.guid = asset::fnv1a_str(name);
    a.type = asset::AssetType::Raw;
    a.codec = asset::Codec::Raw;
    a.residency = asset::Residency::Mmap;
    a.uncompressed_size = static_cast<uint32_t>(bytes.size());
    a.payload = std::move(bytes);
    return a;
}

inline asset::AssetInput texture(uint64_t guid, uint32_t w, uint32_t h) {
    asset::AssetInput a;
    a.guid = guid;
    a.type = asset::AssetType::Texture;
    a.codec = asset::Codec::Raw;
    a.residency = asset::Residency::Stream;
    a.payload.assign(static_cast<std::size_t>(w) * h * 4, 0x5A);
    a.uncompressed_size = static_cast<uint32_t>(a.payload.size());
    a.tex_w = w;
    a.tex_h = h;
    a.tex_format = asset::TEX_FORMAT_RGBA8_UNORM;
    return a;
}

inline bool bake(const Source& s, std::vector<asset::AssetInput>& out, std::string& error) {
    std::vector<uint8_t> maps, visuals, objects, clips;
    tl::MapBakeError map_error;
    if (!tl::bake_maps(s.maps, maps, map_error)) {
        error = "tilemap: " + map_error.message;
        return false;
    }
    if (!tl::bake_visuals(s.visuals, visuals, error) || !tl::bake_objects(s.objects, objects, error) ||
        !gr::bake_clips(s.clips, clips, error))
        return false;
    out = {table("tilemap", std::move(maps)), table("visual", std::move(visuals)),
           table("objects", std::move(objects)), table("clips", std::move(clips)), texture(STREET, 32, 32),
           texture(SKY, 64, 32)};
    for (const uint64_t g : s.images) out.push_back(texture(g, 1, 1));
    for (const std::string& f : s.fighters) {
        std::vector<uint8_t> bytes;
        framework::brawl::FighterBakeError fighter_error;
        const std::string name = f + "_fighter";
        if (!framework::brawl::bake_fighter(name, fighter_text(f), sheet_clips(f), bytes, fighter_error)) {
            error = name + ": " + fighter_error.message;
            return false;
        }
        out.push_back(table(name.c_str(), std::move(bytes)));
    }
    return true;
}

inline asset::AssetInput* find(std::vector<asset::AssetInput>& in, const char* name) {
    const uint64_t guid = asset::fnv1a_str(name);
    const auto it = std::find_if(in.begin(), in.end(), [&](const asset::AssetInput& a) { return a.guid == guid; });
    return it == in.end() ? nullptr : &*it;
}

inline bool open(std::vector<asset::AssetInput> in, asset::BundleView& out) {
    const std::vector<uint8_t> bytes = asset::write_bundle(std::move(in));
    if (bytes.size() > REGION) return false;
    std::memcpy(region, bytes.data(), bytes.size());
    return out.open(region, bytes.size(), false);
}

} // namespace level_fixture
