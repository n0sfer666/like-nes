#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "achievements.hpp"
#include "art.hpp"
#include "audio.hpp"
#include "batch.hpp"
#include "bloom.hpp"
#include "fx.hpp"
#include "gpu.hpp"
#include "input_setup.hpp"
#include "material_fx.hpp"
#include "mobile_app.hpp"
#include "scene_fx.hpp"
#include "shooter_touch.hpp"
#include "surface_frame.hpp"
#include "input_engine.hpp"
#include "game_world.hpp"

namespace game {

// Шутер на телефоне — паритет с десктопом (`live.cpp`): атлас, пресеты ввода и достижения из
// game.bundle, материалы из library.bundle, звук из audio.bundle, частицы, HUD, сюжетные экраны,
// bloom(HDR) + экранные стик и кнопка-огонь. Где лежат бандлы и сейв, решает
// `resolve_asset`/`resolve_save_path`, как на десктопе.
// Мир, звук и достижения живут от init до shutdown; всё, что зависит от формата и размера
// поверхности (bloom, материалы, батч), — от attach_surface до detach_surface.
class MobileGame final : public mobile::Game {
public:
    bool init(GpuContext& gpu) override;
    bool attach_surface(WGPUSurface surface, uint32_t fb_w, uint32_t fb_h,
                        const mobile::SafeArea& safe) override;
    void detach_surface() override;
    void resize(uint32_t fb_w, uint32_t fb_h, const mobile::SafeArea& safe) override;
    void pointer(intptr_t id, mobile::Touch phase, float x_pt, float y_pt) override {
        if (inited_) touch_.pointer(id, phase, x_pt, y_pt);
    }
    void cancel() override {
        if (inited_) touch_.cancel();
    }
    void frame(double dt) override;
    bool wants_frames() const override;
    void suspend() override;
    void resume() override {}
    void shutdown() override;
    std::string touch_state() const override;

private:
    void build_targets(uint32_t fb_w, uint32_t fb_h, const mobile::SafeArea& safe);
    void release_targets();
    void draw();

    GpuContext* gpu_ = nullptr;
    WGPUSurface surface_ = nullptr;
    WGPUTextureFormat fmt_ = WGPUTextureFormat_BGRA8Unorm;
    uint32_t fb_w_ = 0, fb_h_ = 0;
    bool surface_warned_ = false;
    bool lost_ = false;              // поверхность или устройство потеряны: кадр больше не рисуется
    // Цели подняты: detach без attach и повторный release_targets не трогают уже освобождённое.
    bool targets_ = false;
    uint32_t attaches_ = 0;
    Atlas atlas_;
    MaterialFx materials_;
    SceneFx sfx_;
    SpriteBatch batch_;
    Bloom bloom_;
    bool use_bloom_ = false;
    flecs::world world_;
    TrailQuery trails_;              // после world_: запрос гасится раньше мира, которому принадлежит
    GameState gs_;
    Fx fx_;
    FxSink sink_;
    GameAudio audio_;
    // optional: гасится в shutdown, пока живы звук и движок ввода, а не деструктором после них.
    std::optional<Achievements> ach_;
    Controls controls_;
    std::unique_ptr<input::InputEngine> engine_;
    ShooterTouch touch_;
    uint32_t tick_ = 0;
    float vw_ = 0, vh_ = 0;          // мировой вьюпорт (для рендера кнопки)
    bool inited_ = false;
    bool demo_ = false;
};

} // namespace game
