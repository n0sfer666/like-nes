#pragma once

#include <span>
#include <string>
#include <vector>

#include "bundle_writer.hpp"
#include "clip_bake.hpp"
#include "codec.hpp"
#include "tiled_import.hpp"

namespace asset::bakers {

bool texture(const codec::Tools& t, const std::string& src, const char* name, const std::string& tmp,
             std::vector<AssetInput>& out);
// Путь pixel (спека #24, В4): PNG → RGBA8 Raw, nearest, без мипов и внешних инструментов.
bool pixel(const std::vector<uint8_t>& png, const char* name, std::vector<AssetInput>& out,
           std::string& error);
bool shader(const codec::Tools& t, const std::string& src, const char* name, const std::string& ep,
            uint32_t stage, std::vector<AssetInput>& out);
bool audio(const std::string& src, const char* name, bool loop, std::vector<AssetInput>& out);
bool achievements(const std::string& src, std::vector<AssetInput>& out);
bool input_presets(const std::string& src, std::vector<AssetInput>& out);
bool movement(const std::string& src, std::vector<AssetInput>& out);
bool tilemap(const std::string& src, std::vector<AssetInput>& out);
bool atlas_regions(const std::string& src, std::vector<AssetInput>& out);
bool materials(const std::string& src, const std::string& wgsl_src,
               std::vector<AssetInput>& out);
bool lights(const std::string& src, std::vector<AssetInput>& out);
bool levels(std::span<const framework::tiled::Level> levels, std::vector<AssetInput>& out, std::string& error);
bool clips(std::span<const framework::graphics::ClipSrc> clips, std::vector<AssetInput>& out, std::string& error);
void push_table(const char* name, std::vector<uint8_t>&& table, std::vector<AssetInput>& out);
void bulk(const char* name, std::vector<AssetInput>& out);
void synthetic(std::vector<AssetInput>& out);

using asset::TEX_FORMAT_RGBA8_UNORM;

} // namespace asset::bakers
