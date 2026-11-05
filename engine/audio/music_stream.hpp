#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "music_stats.hpp"
#include "music_track.hpp"

namespace audio {
class Mixer;

// Владелец worker и выбора вне rollback snapshot. Mixer удерживает Runtime до своего shutdown.
// want/tick/reset — один control thread; stats можно читать параллельно после init, пока handle жив.
// Уничтожение handle — вне RT: close/join worker без ожидания callback. Handle может пережить Mixer;
// перед уничтожением самого Mixer владелец устройства обязан остановить/join callback.
class MusicStream {
public:
    MusicStream();
    ~MusicStream();
    MusicStream(const MusicStream&) = delete;
    MusicStream& operator=(const MusicStream&) = delete;

    // Холодно, до callbacks: одна успешная привязка на Mixer за всё его время жизни.
    // Metadata копируются; encoded/PCM bytes неизменны и живут до destructor/join этого handle.
    // settle_ticks caller берёт из rollback;0 отказывается. После неудачного init можно повторить.
    bool init(Mixer& mixer, std::span<const MusicTrack> tracks, uint32_t settle_ticks, std::string& error);
    // Unknown/closed/uninitialized отказываются, не стирая предыдущее желание. ID0 разрешён.
    bool want(uint64_t track_id);
    void tick(uint32_t tick);
    // После Session::reset/restart, до нового want/tick. Порог прежний; observer/latest/pending
    // очищаются, уже принятые Runtime wishes и воспроизведение продолжаются. Без want тики не считаются.
    void reset_intent();
    MusicStats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace audio
