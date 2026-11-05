#include "platform_mutex_probe.hpp"

#include <pthread.h>

namespace {
thread_local bool active = false;
thread_local uint64_t count = 0;
}

extern "C" int __real_pthread_mutex_lock(pthread_mutex_t* mutex);
extern "C" int __wrap_pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (active) ++count;
    return __real_pthread_mutex_lock(mutex);
}

namespace platform::mutex_probe {
void begin() { count = 0; active = true; }
uint64_t end() { active = false; return count; }
bool supported() { return true; }
} // namespace platform::mutex_probe
