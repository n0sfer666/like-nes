#include "rumble_window.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <cstdio>

#include "gpu.hpp"
#include "haptic.hpp"
#include "rumble_window_loop.hpp"

namespace rumble {

int run_window(Flow& flow, Hotseat& hotseat, const Credits& credits, const WindowRun& run) {
    if (!glfwInit()) {
        std::fprintf(stderr, "neon-rumble: glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(960, 540, "Neon Rumble", nullptr, nullptr);
    GpuContext gpu;
    WGPUSurface surface = nullptr;
    int rc = 1;
    int drawn = 0;
    if (!window) {
        std::fprintf(stderr, "neon-rumble: window creation failed\n");
    } else if (!(gpu.instance = gpu.create_instance())) {
        std::fprintf(stderr, "neon-rumble: webgpu instance failed\n");
    } else if (!(surface = glfwGetWGPUSurface(gpu.instance, window)) || !gpu.init(surface)) {
        std::fprintf(stderr, "neon-rumble: no surface, adapter or device\n");
    } else {
        rc = window_frame_loop(window, gpu, surface, flow, hotseat, credits, run, drawn);
        if (rc == 0 && drawn == 0) {
            std::fprintf(stderr, "neon-rumble: window closed before the first frame\n");
            rc = 1;
        }
    }
    if (surface) wgpuSurfaceRelease(surface);
    haptic::shutdown();
    gpu.shutdown();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
    if (rc == 0) std::printf("neon-rumble: window run ok, %d frames\n", drawn);
    return rc;
}

} // namespace rumble
