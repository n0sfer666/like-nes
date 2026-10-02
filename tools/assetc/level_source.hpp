#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "tiled_import.hpp"

namespace asset::manifest {

class LevelSource final : public framework::tiled::Source {
public:
    LevelSource(std::string base, const std::map<std::string, framework::tiled::Image>& textures,
                std::vector<std::string>& deps)
        : base_(std::move(base)), textures_(textures), deps_(deps) {}

    bool tileset(const std::string& from, const std::string& rel, std::string& file, std::vector<uint8_t>& bytes,
                 std::string& error) override;
    bool image(const std::string& from, const std::string& rel, framework::tiled::Image& out,
               std::string& error) override;

private:
    bool join(const std::string& from, const std::string& rel, std::string& out, std::string& error) const;

    std::string base_;
    const std::map<std::string, framework::tiled::Image>& textures_;
    std::vector<std::string>& deps_;
};

} // namespace asset::manifest
