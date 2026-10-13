#pragma once
#include <span>

#include "archetype.hpp"
#include "brawl_body.hpp"

namespace framework::brawl {

Role role_of(const Body& b, std::span<const Archetype> kinds);
std::span<const graphics::ClipBox> body_boxes(const Body& b, std::span<const Archetype> kinds, graphics::BoxKind kind);
const graphics::ClipBox* strike_box(const Body& b, std::span<const Archetype> kinds);

} // namespace framework::brawl
