#pragma once
#include <string>

#include "fx_events.hpp"

namespace game {

// Игровое аудио (S9, #3): бейкнутые SFX/музыка → mixer → miniaudio-устройство. Событийно
// из fx-канала. Локально (AUDIO_HAVE_MINIAUDIO); в headless/CI — no-op (init → false).
// pimpl → заголовок без audio/webgpu-зависимостей (live.cpp включает без утечки типов).
class GameAudio {
public:
    ~GameAudio();
    bool init(const std::string& bundle_path);   // false, если нет устройства/бандла
    void on_events(const FxSink& sink);           // раз в кадр: играет SFX по событиям
    void shutdown();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace game
