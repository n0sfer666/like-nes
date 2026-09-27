#include "shader_watch.hpp"

namespace game {

bool ShaderWatch::start(const MaterialFx& materials, const std::string& wgsl_path) {
    if (!materials.ready()) return false;
    return hot_.start(wgsl_path);
}

void ShaderWatch::poll(MaterialFx& materials) {
    if (materials.ready() && hot_.watching()) hot_.poll(materials.cache_, /*timeout_ms=*/0);
}

} // namespace game
