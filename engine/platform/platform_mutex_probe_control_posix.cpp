#include "platform_mutex_probe.hpp"

#include <mutex>
#include <pthread.h>

namespace platform::mutex_probe {
bool exercise_mutexes() {
    pthread_mutex_t native = PTHREAD_MUTEX_INITIALIZER;
    bool ok = pthread_mutex_lock(&native) == 0;
    if (ok) ok = pthread_mutex_unlock(&native) == 0;
    if (pthread_mutex_destroy(&native) != 0) ok = false;

    std::mutex standard;
    try {
        standard.lock();
        standard.unlock();
    } catch (...) {
        ok = false;
    }
    return ok;
}
} // namespace platform::mutex_probe
