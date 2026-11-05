#pragma once

#include "rumble_frame_stats.hpp"
#include "rumble_touch.hpp"
#include "rumble_touch_quads.hpp"

namespace rumble {

struct Credits;
class Hotseat;
struct Play;

class WindowBenchmark {
public:
    bool open(asset::BundleView bundle, Hotseat& hotseat, std::string& why);
    bool bind_touch(asset::BundleView bundle, Hotseat& hotseat, std::string& why);
    void feed(uint32_t width, uint32_t height, uint32_t tick);
    void overlay(const Play& play, const Credits& credits, Layers& layers, LayerStats& st);

    FrameStatsRun frames;

private:
    Touch touch_;
    TouchQuads quads_;
};

} // namespace rumble
