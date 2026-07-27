#include "ipc/mirror.hpp"
#include "ipc/seqlock.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>

#include "platform_args.hpp"
#include "platform_shmem.hpp"

// Game-процесс (Play): открывает shmem RW, публикует read-only зеркало sim-состояния каждый тик
// через seqlock. Режимы: normal (штатная публикация), crash (null-deref после N тиков — крэшит
// ТОЛЬКО себя, граница процессов изолирует редактор), badlayout (несовместимый layout_version →
// редактор обязан отвергнуть). argv: <name> <mode> <count>.
//
// Сегмент открывается ПО ИМЕНИ из командной строки, а не по унаследованному дескриптору
// (решение 2 спеки #13): наследование хендлов на Windows — отдельная церемония с
// bInheritHandles, и путь запуска игры ветвился бы по ОС ровно там, где он должен быть общим.
using namespace ide::ipc;

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc < 4) return 2;
    std::string name = argv[1];
    std::string mode = argv[2];
    uint32_t count = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
    if (count > MIRROR_CAPACITY) count = MIRROR_CAPACITY;

    platform::SharedMemory m;
    if (!m.open(name, sizeof(MirrorBuffer), /*create=*/false, /*writable=*/true)) return 3;
    auto* buf = static_cast<MirrorBuffer*>(m.writable_data());

    buf->header.seq.store(0);   // атомик; заголовок-скаляры публикуются ПОД seqlock (см. цикл)
    const uint32_t layout = (mode == "badlayout") ? 999u : MIRROR_LAYOUT_VERSION;
    const uint32_t crash_tick = (mode == "crash") ? 20u : 0xFFFFFFFFu;
    const uint32_t max_ticks = 3000;   // ~3с; редактор обычно останавливает (kill) раньше

    for (uint32_t tick = 1; tick <= max_ticks; ++tick) {
        seq_write_begin(buf->header.seq);
        buf->header.magic = MIRROR_MAGIC;          // заголовок под seqlock → reader без гонки
        buf->header.layout_version = layout;
        buf->header.schema_hash = mirror_schema_hash();
        buf->header.capacity = MIRROR_CAPACITY;
        buf->header.count = count;
        for (uint32_t i = 0; i < count; ++i) {
            buf->entities[i].guid = 1000 + i;
            buf->entities[i].px = static_cast<int32_t>((i % 128) * 65536 + tick);
            buf->entities[i].py = static_cast<int32_t>((i / 128) * 65536);
            buf->entities[i].vx = 0;
            buf->entities[i].vy = 0;
            buf->entities[i].gen = tick;
        }
        seq_write_end(buf->header.seq);

        if (tick == crash_tick) {
            volatile int* p = nullptr;
            *p = 42;   // SIGSEGV — изолирован границей процесса
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    m.close();   // child не owner → без unlink
    return 0;
}
