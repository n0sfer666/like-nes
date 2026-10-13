#pragma once

#include <cstdint>

#include "ai_brain.hpp"
#include "body_pool.hpp"
#include "brawl_body.hpp"
#include "brawl_step.hpp"
#include "projectile.hpp"
#include "stage_sections.hpp"

namespace rumble {

constexpr int32_t KNIFE_SPEED = 3;
constexpr uint16_t KNIFE_LIFE = 90;
constexpr uint16_t KNIFE_EVERY = 90;
constexpr int32_t KNIFE_FROM = 12;

framework::brawl::Shot knife_from(const framework::brawl::Body& thrower);

// Мозг в SHOOTER_THROW выпускает нож тела в порядке мозгов и возвращается в SHOOTER_START с таймером
// KNIFE_EVERY. Зовётся после шага ИИ и до шага сцены: нож встаёт в хвост пула, под нулевую команду, и
// летит уже в этом тике.
void throw_knives(framework::ai::Brains& brains, framework::brawl::BodyPool& pool,
                  framework::brawl::StageSections& sections, const framework::brawl::BrawlWorld& world);

} // namespace rumble
