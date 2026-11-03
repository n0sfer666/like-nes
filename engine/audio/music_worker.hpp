#pragma once
#include <array>
#include <vector>

#include "music_decoder.hpp"
#include "music_transport.hpp"

namespace audio {
// Private worker logic: cold init затем один owning thread вызывает pump. Manual tests гоняют
// тот же pump без std::thread. Поток/stop/join будут у MusicStream, не у callback/storage.
class MusicWorker {
public:
    explicit MusicWorker(MusicTransport& transport) : transport_(transport) {}
    ~MusicWorker();
    bool init(std::span<const MusicTrack> tracks, std::string& error);
    bool pump(); // false только closed/неинициализирован; не ждёт места в кольце/команды
private:
    struct Slot {
        MusicDecoder decoder;
        std::array<StereoFrame, MusicDecoder::BLOCK_FRAMES> block;
        uint64_t generation = 0;
        uint32_t offset = 0, count = 0, primed = 0;
        bool active = false;
    };
    void close_slot(uint32_t slot);
    void fail_slot(uint32_t slot, uint64_t generation);
    void load(const MusicLoad& request);
    MusicTransport& transport_;
    std::vector<MusicTrack> tracks_;
    std::array<Slot, MUSIC_SLOTS> slots_;
    bool initialized_ = false;
};
}
