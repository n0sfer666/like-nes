#include "scene_fx.hpp"

#include <cstdio>

namespace game {
namespace {

constexpr float BURN_FROM = 0.25f;   // доля HP босса, ниже которой он начинает выгорать

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

} // namespace

// Имена и слоты берутся ОДИН РАЗ: искать строку в таблице каждый кадр — это разбор в кадре, от
// которого весь zero-parse путь и уводит. Смещения спрашиваются у таблицы, а не пишутся числом:
// перестановка строк в `library.mat` иначе молча красила бы не тот слот.
bool SceneFx::bind(MaterialFx* fx) {
    fx_ = nullptr;
    if (fx == nullptr || !fx->ready()) return false;
    const uint32_t n = fx->count();
    flash_ = fx->find("flash_red");
    outline_ = fx->find("outline_danger");
    dissolve_ = fx->find("dissolve_ash");
    if (flash_ >= n || outline_ >= n || dissolve_ >= n) {
        std::fprintf(stderr, "[game] materials: library has no flash_red/outline_danger/dissolve_ash\n");
        return false;
    }
    strength_slot_ = fx->slot(flash_, "strength");
    threshold_slot_ = fx->slot(dissolve_, "threshold");
    if (strength_slot_ < 0 || threshold_slot_ < 0) {
        std::fprintf(stderr, "[game] materials: library lost a parameter the sample sets\n");
        return false;
    }
    fx_ = fx;
    return true;
}

uint32_t SceneFx::enemy(float approach, float p[mat::PARAM_BLOCK_FLOATS]) const {
    fx_->params(flash_, p);
    p[strength_slot_] = clamp01(approach) * 0.75f;
    return flash_;
}

uint32_t SceneFx::boss(float hp_frac, float p[mat::PARAM_BLOCK_FLOATS]) const {
    if (hp_frac > BURN_FROM) {
        fx_->params(outline_, p);
        return outline_;
    }
    fx_->params(dissolve_, p);
    p[threshold_slot_] = clamp01((BURN_FROM - hp_frac) / BURN_FROM) * 0.85f;
    return dissolve_;
}

} // namespace game
