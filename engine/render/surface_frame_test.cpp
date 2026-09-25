#include "surface_frame.hpp"

#include <cstdio>

// Аудит #21 A·3·6 и A·3·11: статус поверхности → действие петли. Устаревшая поверхность чинится
// переконфигурацией, а не ожиданием; потерянное устройство выходит при любом статусе, иначе
// переконфигурация на нём крутилась бы вечно. Без GPU: функция чистая.
namespace {

int fails = 0;

void expect(WGPUSurfaceGetCurrentTextureStatus st, bool lost, FrameAction want, const char* what) {
    if (frame_action(st, lost) != want) {
        std::printf("[surface-frame] FAIL: %s\n", what);
        ++fails;
    }
}

} // namespace

int main() {
    expect(WGPUSurfaceGetCurrentTextureStatus_Success, false, FrameAction::Draw, "success draws");
    expect(WGPUSurfaceGetCurrentTextureStatus_Timeout, false, FrameAction::Skip, "timeout skips");
    expect(WGPUSurfaceGetCurrentTextureStatus_Outdated, false, FrameAction::Reconfigure,
           "outdated reconfigures");
    expect(WGPUSurfaceGetCurrentTextureStatus_Lost, false, FrameAction::Reconfigure,
           "lost surface reconfigures");
    expect(WGPUSurfaceGetCurrentTextureStatus_OutOfMemory, false, FrameAction::Quit,
           "out of memory quits");
    expect(WGPUSurfaceGetCurrentTextureStatus_DeviceLost, false, FrameAction::Quit,
           "device-lost status quits");
    expect(WGPUSurfaceGetCurrentTextureStatus_Outdated, true, FrameAction::Quit,
           "lost device quits even on outdated");
    expect(WGPUSurfaceGetCurrentTextureStatus_Success, true, FrameAction::Quit,
           "lost device quits even on success");
    if (fails) {
        std::printf("surface-frame: FAIL (%d)\n", fails);
        return 1;
    }
    std::printf("surface-frame: PASS - 8 statuses\n");
    return 0;
}
