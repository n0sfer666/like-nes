#include "ipc/mirror.hpp"
#include "ipc/seqlock.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>

// Same-process стресс seqlock: 1 writer + 3 reader-потока над общим MirrorBuffer. Writer штампует
// все N сущностей одним поколением (gen); reader после консистентного чтения проверяет, что все
// сущности снапшота имеют ОДИН gen (torn-read → разные gen). Функциональная валидация корректности
// seqlock под контеншеном (в реальных гейтах writer/reader — разные процессы через mmap).
using namespace ide::ipc;

static MirrorBuffer g_buf;

int main() {
    g_buf.header.magic = MIRROR_MAGIC;
    g_buf.header.layout_version = MIRROR_LAYOUT_VERSION;
    g_buf.header.schema_hash = mirror_schema_hash();
    g_buf.header.capacity = MIRROR_CAPACITY;
    const uint32_t N = 10000;
    g_buf.header.count = N;
    g_buf.header.seq.store(0);

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> torn{0}, reads{0};
    const uint32_t GEN_LIMIT = 500;   // writer-ограничение → детерм. по времени (~0.5с)

    // Writer пишет с межкадровой паузой (~реальность: game публикует зеркало с частотой кадра,
    // не в busy-loop) → readers получают реальные окна и накапливают много консистентных чтений;
    // границы окон стрессят torn-детект.
    std::thread writer([&] {
        for (uint32_t gen = 1; gen <= GEN_LIMIT; ++gen) {
            seq_write_begin(g_buf.header.seq);
            for (uint32_t i = 0; i < N; ++i) {
                g_buf.entities[i].guid = 1000 + i;
                g_buf.entities[i].px = static_cast<int32_t>(gen);
                g_buf.entities[i].gen = gen;
            }
            seq_write_end(g_buf.header.seq);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        stop.store(true);
    });

    auto reader_fn = [&] {
        std::vector<MirrorEntity> local(MIRROR_CAPACITY);
        uint32_t cnt = 0;
        while (!stop.load()) {
            bool ok = seq_read(g_buf.header.seq, [&] {
                cnt = g_buf.header.count;
                for (uint32_t i = 0; i < cnt; ++i) local[i] = g_buf.entities[i];
            });
            if (!ok || cnt == 0) continue;
            uint32_t g = local[0].gen;
            for (uint32_t i = 1; i < cnt; ++i)
                if (local[i].gen != g) { torn.fetch_add(1); break; }
            reads.fetch_add(1);
        }
    };

    std::vector<std::thread> readers;
    for (int i = 0; i < 3; ++i) readers.emplace_back(reader_fn);
    writer.join();
    for (auto& t : readers) t.join();

    std::printf("seqlock reads: %llu, torn snapshots: %llu\n",
                static_cast<unsigned long long>(reads.load()),
                static_cast<unsigned long long>(torn.load()));
    bool pass = (torn.load() == 0) && (reads.load() > 1000);
    std::printf("ipc-seqlock: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
