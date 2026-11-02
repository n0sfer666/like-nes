#include "android_window.hpp"

#include <android/looper.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <map>
#include <memory>
#include <mutex>

#include "android_window_jni.hpp"

// native_app_glue запускает игровой поток. Наш вход оборачивает только UI-callbacks;
// instance и весь протокол завершения остаются у glue.
extern "C" void like_nes_glue_onCreate(ANativeActivity*, void*, size_t);

namespace {

struct Window {
    ANativeActivity* activity;
    ANativeActivityCallbacks previous{};
    AndroidWindowSnapshot snapshot;
    int pipe[2] = {-1, -1};
    ALooper* looper = nullptr;
    bool warned = false;
};

std::mutex windows_mutex;
std::map<ANativeActivity*, std::unique_ptr<Window>> windows;

// UI-callbacks последовательны; destroy удаляет Window лишь после join игрового потока в glue.
Window* lookup(ANativeActivity* activity) {
    const std::lock_guard lock(windows_mutex);
    const auto it = windows.find(activity);
    return it != windows.end() ? it->second.get() : nullptr;
}

void sample(Window& w, bool immersive = false) {
    AndroidWindowSnapshot next;
    const bool configured = !immersive || android_window_immersive(w.activity);
    if (!configured || !android_window_sample(w.activity, next)) {
        if (!w.warned) std::fprintf(stderr, "[game] Android window JNI failed\n");
        w.warned = true;
        return;
    }
    const std::lock_guard lock(windows_mutex);
    w.snapshot = next;
}

int requested(int fd, int events, void* data) {
    Window& w = *static_cast<Window*>(data);
    if ((events & ALOOPER_EVENT_INPUT) == 0) return 0;
    char bytes[64];
    while (read(fd, bytes, sizeof bytes) > 0) {}
    sample(w);
    return 1;
}

void focus(ANativeActivity* a, int has_focus) {
    Window& w = *lookup(a);
    sample(w, has_focus != 0);
    if (w.previous.onWindowFocusChanged) w.previous.onWindowFocusChanged(a, has_focus);
}

void resume(ANativeActivity* a) {
    Window& w = *lookup(a);
    sample(w, true);
    if (w.previous.onResume) w.previous.onResume(a);
}

void created(ANativeActivity* a, ANativeWindow* window) {
    Window& w = *lookup(a);
    sample(w, true);
    if (w.previous.onNativeWindowCreated) w.previous.onNativeWindowCreated(a, window);
}

void destroy(ANativeActivity* a) {
    Window& w = *lookup(a);
    if (w.previous.onDestroy) w.previous.onDestroy(a); // glue ждёт выхода android_main
    if (w.looper) {
        ALooper_removeFd(w.looper, w.pipe[0]);
        ALooper_release(w.looper);
    }
    for (int fd : w.pipe) if (fd >= 0) close(fd);
    const std::lock_guard lock(windows_mutex);
    windows.erase(a);
}

} // namespace

AndroidWindowSnapshot android_window_read(ANativeActivity* a) {
    const std::lock_guard lock(windows_mutex);
    const auto it = windows.find(a);
    return it != windows.end() ? it->second->snapshot : AndroidWindowSnapshot{};
}

void android_window_request(ANativeActivity* a) {
    const std::lock_guard lock(windows_mutex);
    const auto it = windows.find(a);
    if (it == windows.end() || it->second->pipe[1] < 0) return;
    const char byte = 1;
    // Неблокирующая очередь: полный pipe уже содержит запрос. UI объединяет накопленные запросы.
    const ssize_t sent = write(it->second->pipe[1], &byte, 1);
    (void)sent;
}

extern "C" void ANativeActivity_onCreate(ANativeActivity* a, void* saved, size_t size) {
    auto state = std::make_unique<Window>();
    Window& w = *state;
    w.activity = a;
    if (pipe2(w.pipe, O_NONBLOCK | O_CLOEXEC) == 0) {
        w.looper = ALooper_forThread();
        if (w.looper && ALooper_addFd(w.looper, w.pipe[0], ALOOPER_POLL_CALLBACK,
                                      ALOOPER_EVENT_INPUT, requested, &w) == 1) {
            ALooper_acquire(w.looper);
        } else {
            w.looper = nullptr;
            for (int fd : w.pipe) close(fd);
            w.pipe[0] = w.pipe[1] = -1;
        }
    }
    if (!w.looper) std::fprintf(stderr, "[game] Android UI poll unavailable\n");
    sample(w, true);
    {
        const std::lock_guard lock(windows_mutex);
        windows.emplace(a, std::move(state));
    }
    like_nes_glue_onCreate(a, saved, size);
    w.previous = *a->callbacks;
    a->callbacks->onWindowFocusChanged = focus;
    a->callbacks->onNativeWindowCreated = created;
    a->callbacks->onResume = resume;
    a->callbacks->onDestroy = destroy;
}
