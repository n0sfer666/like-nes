#include "ipc/mirror.hpp"
#include "ipc/seqlock.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

// Гейт 8: замер доставки 10k-снапшота consumer'у — shmem (seqlock, zero-serialize) vs unix-socket
// (memcpy+syscall+kernel-копии). p50/p99 latency + байт/кадр. Producer/consumer — 2 потока,
// lockstep (producer публикует k, ждёт ack). Общие часы steady_clock. Фиксирует выбор транспорта
// в ADR 0007 (гейт 8): гипотеза — shmem константно дешевле, socket растёт с payload.
using namespace ide::ipc;

namespace {
using Clock = std::chrono::steady_clock;
int64_t now_ns() { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count(); }

void pctl(std::vector<double>& v, const char* tag, size_t bytes) {
    std::sort(v.begin(), v.end());
    double p50 = v[v.size() / 2];
    double p99 = v[v.size() * 99 / 100];
    std::printf("%-8s p50=%8.3f us  p99=%8.3f us  bytes/frame=%zu\n", tag, p50, p99, bytes);
}

bool recv_all(int fd, void* buf, size_t n) {
    auto* p = static_cast<uint8_t*>(buf);
    while (n) {
        ssize_t r = recv(fd, p, n, 0);
        if (r <= 0) return false;
        p += r; n -= static_cast<size_t>(r);
    }
    return true;
}
bool send_all(int fd, const void* buf, size_t n) {
    auto* p = static_cast<const uint8_t*>(buf);
    while (n) {
        ssize_t r = send(fd, p, n, 0);
        if (r <= 0) return false;
        p += r; n -= static_cast<size_t>(r);
    }
    return true;
}
} // namespace

static MirrorBuffer g_buf;

int main() {
    const uint32_t N = 10000;
    const int ITERS = 2000;
    const size_t frame_bytes = static_cast<size_t>(N) * sizeof(MirrorEntity);

    g_buf.header.count = N;
    g_buf.header.seq.store(0);
    for (uint32_t i = 0; i < N; ++i) g_buf.entities[i].guid = 1000 + i;

    // ---- shmem (seqlock) ----
    {
        std::atomic<int64_t> t0{0};
        std::atomic<uint32_t> pub{0}, ack{0};
        std::vector<double> lat;
        lat.reserve(ITERS);

        std::thread cons([&] {
            std::vector<MirrorEntity> local(N);
            for (int k = 1; k <= ITERS; ++k) {
                while (pub.load(std::memory_order_acquire) != (uint32_t)k) { /* spin */ }
                seq_read(g_buf.header.seq, [&] {
                    for (uint32_t i = 0; i < N; ++i) local[i] = g_buf.entities[i];
                });
                int64_t t1 = now_ns();
                lat.push_back((t1 - t0.load(std::memory_order_relaxed)) / 1000.0);
                ack.store((uint32_t)k, std::memory_order_release);
            }
        });
        for (int k = 1; k <= ITERS; ++k) {
            t0.store(now_ns(), std::memory_order_relaxed);   // produce полного кадра включён (симметрия)
            seq_write_begin(g_buf.header.seq);
            for (uint32_t i = 0; i < N; ++i) {
                g_buf.entities[i].guid = 1000 + i;
                g_buf.entities[i].px = (int32_t)k;
                g_buf.entities[i].py = 0; g_buf.entities[i].vx = 0; g_buf.entities[i].vy = 0;
                g_buf.entities[i].gen = (uint32_t)k;
            }
            seq_write_end(g_buf.header.seq);
            pub.store((uint32_t)k, std::memory_order_release);
            while (ack.load(std::memory_order_acquire) != (uint32_t)k) { /* spin */ }
        }
        cons.join();
        pctl(lat, "shmem", 0);   // 0 сериализации: consumer читает из mmap, copies/frame = 1 (read-out)
    }

    // ---- unix socket (SOCK_STREAM) ----
    {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { std::printf("socketpair fail\n"); return 3; }
        std::atomic<int64_t> t0{0};
        std::vector<double> lat;
        lat.reserve(ITERS);

        std::thread cons([&] {
            std::vector<MirrorEntity> rbuf(N);
            for (int k = 1; k <= ITERS; ++k) {
                if (!recv_all(sv[1], rbuf.data(), frame_bytes)) break;
                int64_t t1 = now_ns();
                lat.push_back((t1 - t0.load(std::memory_order_relaxed)) / 1000.0);
                uint8_t a = 1;
                send(sv[1], &a, 1, 0);   // ack (lockstep)
            }
        });
        std::vector<MirrorEntity> sbuf(N);
        for (int k = 1; k <= ITERS; ++k) {
            t0.store(now_ns(), std::memory_order_relaxed);   // produce (сериализация) включён — симметрия с shmem
            for (uint32_t i = 0; i < N; ++i) { sbuf[i] = g_buf.entities[i]; sbuf[i].gen = (uint32_t)k; }
            send_all(sv[0], sbuf.data(), frame_bytes);   // serialize=memcpy + syscall + kernel copy
            uint8_t a; recv(sv[0], &a, 1, 0);
        }
        cons.join();
        close(sv[0]); close(sv[1]);
        pctl(lat, "socket", frame_bytes);   // copies/frame >= 2 (user->kernel->user) + syscall
    }

    std::printf("ide-ipc-bench: PASS\n");   // замер, не гейт-fail; числа → ADR 0007 гейт 8
    return 0;
}
