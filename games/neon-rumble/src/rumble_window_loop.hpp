#pragma once

#include <webgpu/webgpu.h>

struct GLFWwindow;
struct GpuContext;

namespace rumble {

struct Credits;
class Flow;
class Hotseat;
struct WindowRun;

int window_frame_loop(GLFWwindow* window, GpuContext& gpu, WGPUSurface surface, Flow& flow,
                      Hotseat& hotseat, const Credits& credits, const WindowRun& run, int& drawn);

} // namespace rumble
