#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace rumble {

struct FrameMusicStats {
    uint64_t underruns = 0;
    uint64_t audible_callbacks = 0;
    bool valid = false;
};

class FrameMusic {
public:
    FrameMusic();
    ~FrameMusic();
    bool open(const std::string& path, bool null_audio, std::string& why);
    void tick();
    void poll();
    void suspend();
    void resume();
    FrameMusicStats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rumble
