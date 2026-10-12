#pragma once
#include <cstdint>

#include "rumble_banner.hpp"
#include "rumble_credits.hpp"
#include "rumble_fighter_quads.hpp"
#include "rumble_hotseat.hpp"
#include "rumble_layers.hpp"
#include "rumble_menu.hpp"
#include "rumble_menu_quads.hpp"
#include "rumble_play.hpp"
#include "viewport_fit.hpp"

namespace rumble {

// Кадр окна из квадов сцены на вершине стека: меню — пункты, уровень — слои, бойцы и надпись лобби;
// титры поверх обоих. `shown` — страница титров + 1 (0 — скрыты), `overlay` — оверлей глубины F3.
class Screen {
public:
    uint32_t shown = 0;
    bool overlay = false;

    // Смена сцены: титры скрыты, камера без точки — новый уровень не начнёт с позиции прошлого.
    void reset();
    LayerStats menu(const Menu& m, const Credits& credits, const framework::graphics::ViewportFit& fit,
                    uint64_t tick);
    LayerStats level(const Play& play, const Hotseat& hotseat, const Credits& credits,
                     const framework::graphics::ViewportFit& fit, uint64_t tick);
    const Layers& layers() const { return layers_; }

private:
    void credits_page(const Credits& credits, const framework::graphics::ViewportFit& fit, LayerStats& st,
                      uint32_t font_texture, uint32_t solid_texture, uint64_t tick);

    Layers layers_;
    FighterQuads fighters_;
    CreditsQuads credits_;
    BannerQuads banner_;
    MenuQuads menu_;
    // Пока на улице никого, камера держит последнюю точку: без неё слои ушли бы в автопрогон.
    framework::Vec2 follow_;
    bool following_ = false;
    bool fighter_warned_ = false;
    bool credits_warned_ = false;
    bool menu_warned_ = false;
};

} // namespace rumble
