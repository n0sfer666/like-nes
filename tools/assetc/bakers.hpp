#pragma once

#include <string>
#include <vector>

#include "bundle_writer.hpp"
#include "codec.hpp"

namespace asset::bakers {

bool texture(const codec::Tools& t, const std::string& src, const char* name, const std::string& tmp,
             std::vector<AssetInput>& out);
bool shader(const codec::Tools& t, const std::string& src, const char* name, const std::string& ep,
            uint32_t stage, std::vector<AssetInput>& out);
bool audio(const std::string& src, const char* name, bool loop, std::vector<AssetInput>& out);
bool achievements(const std::string& src, std::vector<AssetInput>& out);
void bulk(const char* name, std::vector<AssetInput>& out);
void synthetic(std::vector<AssetInput>& out);

} // namespace asset::bakers
