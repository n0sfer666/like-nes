#include "wgpu_error.hpp"

#include <webgpu/wgpu.h>

namespace mat::detail {
namespace {

struct Capture {
    bool fired = false;
    std::string message;
};

void on_error(WGPUErrorType type, char const* message, void* ud) {
    Capture* c = static_cast<Capture*>(ud);
    c->fired = true;
    if (type != WGPUErrorType_NoError && message) c->message = message;
}

} // namespace

void error_scope_begin(WGPUDevice device) {
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
}

std::string error_scope_end(WGPUDevice device) {
    Capture c;
    wgpuDevicePopErrorScope(device, on_error, &c);
    // На wgpu-native коллбэк срабатывает прямо в pop; опрос стоит для реализаций, где он отложен.
    if (!c.fired) wgpuDevicePoll(device, true, nullptr);
    if (!c.fired) return "error scope did not answer: the result of this call is unknown";
    return c.message;
}

} // namespace mat::detail
