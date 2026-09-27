#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <optional>

#include "achievements.hpp"
#include "art.hpp"
#include "audio.hpp"
#include "batch.hpp"
#include "bloom.hpp"
#include "fx.hpp"
#include "gpu.hpp"
#include "input_setup.hpp"
#include "material_fx.hpp"
#include "scene_fx.hpp"
#include "surface_frame.hpp"
#include "input_engine.hpp"
#include "game_world.hpp"

namespace game {

// Общий игровой кадр mobile-шеллов (iOS/Android) — паритет с десктопом (`live.cpp`): атлас, пресеты
// ввода и достижения из game.bundle, материалы из library.bundle, звук из audio.bundle, частицы,
// HUD, сюжетные экраны, bloom(HDR) + экранная кнопка-огонь. Где лежат бандлы и сейв, решает
// `resolve_asset`/`resolve_save_path`, как на десктопе. Шеллы остаются тонкими: платформенный
// тач/lifecycle → pointer/frame/shutdown.
// Мульти-тач: левая зона = виртуальный стик (движение), правый-нижний круг = огонь (PadA).
class MobileGame {
public:
    enum class Touch { Down, Move, Up };

    bool init(GpuContext& gpu, WGPUSurface surface, uint32_t fb_w, uint32_t fb_h);
    void set_demo(bool on) { demo_ = on; }
    // id — intptr_t: iOS отдаёт адрес UITouch целиком, усечение до int склеило бы два касания.
    void pointer(intptr_t id, Touch phase, float px, float py, float view_w, float view_h);
    void cancel();   // системный CANCEL (шторка/звонок): сбросить все активные касания
    void frame(WGPUSurface surface);
    // Уход в фон: система убивает фоновый процесс без dealloc и onDestroy, и статы достижений,
    // копящиеся до автосейва, пропали бы. Сохраняет, ничего не освобождая.
    void suspend();
    void shutdown();

private:
    void post_axis(fix32 x, fix32 y);
    void post_fire(bool down);
    void demo_drive();
    void push_fire_button();
    void draw(WGPUSurface surface);

    GpuContext* gpu_ = nullptr;
    WGPUTextureFormat fmt_ = WGPUTextureFormat_BGRA8Unorm;
    uint32_t fb_w_ = 0, fb_h_ = 0;
    bool surface_warned_ = false;
    bool lost_ = false;              // поверхность или устройство потеряны: кадр больше не рисуется
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
    // optional: повторный init (Android пересоздаёт окно) обязан начать с чистого наблюдателя.
    std::optional<Achievements> ach_;
    Controls controls_;
    input::InputEngine* engine_ = nullptr;
    uint32_t tick_ = 0;
    uint64_t seq_ = 0;
    float vw_ = 0, vh_ = 0;          // мировой вьюпорт (для рендера кнопки)
    intptr_t stick_id_ = -1, fire_id_ = -1;
    float stick_ox_ = 0, stick_oy_ = 0;
    bool inited_ = false;
    bool demo_ = false;
    bool fire_down_ = false;
};

} // namespace game
