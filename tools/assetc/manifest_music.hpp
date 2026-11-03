#pragma once
#include "assetc_manifest.hpp"
#include "manifest_bake.hpp"
#include "music_bake.hpp"

namespace asset::manifest {
bool music_options(const std::vector<std::string>& fields, uint32_t line, Error& error, MusicOptions& out);
bool music_shape(const std::vector<std::string>& fields, uint32_t line, Error& error);
bool gather_music(Bake& bake, const Record& record, const std::string& file,
                  std::vector<framework::music::MusicSrc>& tracks);
bool finish_music(Bake& bake, std::span<const framework::music::MusicSrc> tracks, uint32_t line);
}
