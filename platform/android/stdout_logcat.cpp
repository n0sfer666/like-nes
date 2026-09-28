#include "stdout_logcat.hpp"

#include <android/log.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <string>
#include <thread>

namespace game {
namespace {

void pump(int fd) {
    std::string line;
    char buf[512];
    for (;;) {
        const ssize_t n = read(fd, buf, sizeof buf);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] != '\n') {
                line.push_back(buf[i]);
                continue;
            }
            __android_log_write(ANDROID_LOG_INFO, "like-nes", line.c_str());
            line.clear();
        }
    }
    if (!line.empty()) __android_log_write(ANDROID_LOG_INFO, "like-nes", line.c_str());
    // Без читателя пишущий в полный канал встал бы навсегда (ANR); закрытый конец даёт ему EPIPE.
    close(fd);
}

bool redirect() {
    int fds[2];
    if (pipe(fds) != 0) return false;
    const int old_out = dup(STDOUT_FILENO), old_err = dup(STDERR_FILENO);
    // Построчная буферизация: иначе printf копит 4 КБ, и строка старта доезжает в logcat, когда
    // игра уже давно идёт — или не доезжает вовсе, если процесс убит раньше.
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    const bool ok = dup2(fds[1], STDOUT_FILENO) >= 0 && dup2(fds[1], STDERR_FILENO) >= 0;
    close(fds[1]);
    if (!ok) {
        // Половина перенаправления оставила бы fd 1 каналом без читателя — возвращаем прежние.
        if (old_out >= 0) dup2(old_out, STDOUT_FILENO);
        if (old_err >= 0) dup2(old_err, STDERR_FILENO);
        close(fds[0]);
    } else {
        std::thread(pump, fds[0]).detach();
    }
    if (old_out >= 0) close(old_out);
    if (old_err >= 0) close(old_err);
    return ok;
}

} // namespace

bool stdout_to_logcat() {
    // android_main входит заново в том же процессе, когда активность пересоздаётся; второй setvbuf
    // после вывода — неопределённое поведение, и перенаправление живёт ровно одно на процесс.
    static const bool redirected = redirect();
    return redirected;
}

} // namespace game
