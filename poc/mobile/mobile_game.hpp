#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <string>

#include "art.hpp"
#include "audio.hpp"
#include "batch.hpp"
#include "bloom.hpp"
#include "fx.hpp"
#include "gpu.hpp"
#include "engine.hpp"
#include "world.hpp"

namespace game {

// Общий игровой кадр mobile-шеллов (iOS/Android) — полный desktop-паритет (S10):
// sim + частицы + HUD + сюжетные экраны + bloom(HDR) + аудио(graceful) + экранная кнопка-огонь.
// Шеллы остаются тонкими: платформенный тач/lifecycle → pointer_event/frame/shutdown.
// Мульти-тач: левая зона = виртуальный стик (движение), правый-нижний круг = огонь (PadA).
class MobileGame {
public:
    enum class Touch { Down, Move, Up };

    bool init(GpuContext& gpu, WGPUSurface surface, uint32_t fb_w, uint32_t fb_h,
              const std::string& audio_bundle);
    void set_demo(bool on) { demo_ = on; }
    void pointer(int id, Touch phase, float px, float py, float view_w, float view_h);
    void cancel();   // системный CANCEL (шторка/звонок): сбросить все активные касания
    void frame(WGPUSurface surface);
    void shutdown();

private:
    void post_axis(fix32 x, fix32 y);
    void post_fire(bool down);
    void demo_drive();
    void push_fire_button();

    GpuContext* gpu_ = nullptr;
    WGPUTextureFormat fmt_ = WGPUTextureFormat_BGRA8Unorm;
    Atlas atlas_;
    SpriteBatch batch_;
    Bloom bloom_;
    bool use_bloom_ = false;
    flecs::world world_;
    GameState gs_;
    Fx fx_;
    FxSink sink_;
    GameAudio audio_;
    input::ActionMap map_;
    input::InputEngine* engine_ = nullptr;
    uint32_t tick_ = 0;
    uint64_t seq_ = 0;
    float vw_ = 0, vh_ = 0;          // мировой вьюпорт (для рендера кнопки)
    int stick_id_ = -1, fire_id_ = -1;
    float stick_ox_ = 0, stick_oy_ = 0;
    bool inited_ = false;
    bool demo_ = false;
    bool fire_down_ = false;
};

} // namespace game
