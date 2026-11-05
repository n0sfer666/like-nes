#include "platform_mutex_probe.hpp"

#include <pthread.h>

namespace {
thread_local bool active = false;
thread_local uint64_t count = 0;
}

extern "C" int probe_pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (active) ++count;
    return pthread_mutex_lock(mutex);
}

// Dyld заменяет вызовы из остальных images; вызов оригинала из interposer остаётся прямым.
__attribute__((used)) static const struct {
    const void* replacement;
    const void* original;
} interpose[] __attribute__((section("__DATA,__interpose"))) = {{
    reinterpret_cast<const void*>(&probe_pthread_mutex_lock),
    reinterpret_cast<const void*>(&pthread_mutex_lock)
}};

namespace platform::mutex_probe {
void begin() { count = 0; active = true; }
uint64_t end() { active = false; return count; }
bool supported() { return true; }
} // namespace platform::mutex_probe
