#include "ipc/mirror.hpp"
#include "ipc/seqlock.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "platform_args.hpp"
#include "platform_process.hpp"
#include "platform_shmem.hpp"

// Гейты 3 (Play-spawn + read-only зеркало 10k), 4 (крэш-изоляция), layout-drift (часть гейта 8).
// Редактор = parent: создаёт сегмент (owner, RO-мап), запускает game_child через platform::Child,
// читает консистентный снапшот через seqlock. Крэш child не трогает parent. Layout-drift → reject.
//
// fork+exec здесь больше нет (спека #13): ребёнок всегда отдельный процесс по argv, а исход
// «упал» против «остановлен» отдаёт шов (ExitKind) — на Windows разбирать сигналы было бы нечем.
using namespace ide::ipc;

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("  FAIL: %s\n", w); ++failures; } }

bool spawn(platform::Child& child, const std::string& child_path, const std::string& name,
           const char* mode, uint32_t count) {
    return child.spawn({child_path, name, mode, std::to_string(count)});
}

enum class ReadResult { NotReady, LayoutDrift, Ok };

// Все поля читаются ПОД seqlock (acquire/retry) → нет гонки с writer'ом; заголовок валидируется
// после консистентного захвата. NotReady = ещё не опубликовано; LayoutDrift = несовместимый layout
// (не читаем как валидный); Ok = консистентный снапшот 10k.
ReadResult read_mirror(const MirrorBuffer* buf, std::vector<MirrorEntity>& out, uint32_t& count) {
    count = 0;
    uint32_t magic_r = 0, layout_r = 0, cnt = 0;
    uint64_t schema_r = 0;
    bool consistent = seq_read(buf->header.seq, [&] {
        magic_r = buf->header.magic;
        layout_r = buf->header.layout_version;
        schema_r = buf->header.schema_hash;
        cnt = buf->header.count;
        if (cnt > MIRROR_CAPACITY) cnt = MIRROR_CAPACITY;
        for (uint32_t i = 0; i < cnt; ++i) out[i] = buf->entities[i];
    });
    if (!consistent || magic_r != MIRROR_MAGIC) return ReadResult::NotReady;
    if (layout_r != MIRROR_LAYOUT_VERSION || schema_r != mirror_schema_hash())
        return ReadResult::LayoutDrift;
    if (cnt == 0) return ReadResult::NotReady;
    count = cnt;
    return ReadResult::Ok;
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    if (argc < 2) { std::printf("usage: play_spawn_test <game_child_path>\n"); return 2; }
    const std::string child = argv[1];
    const uint32_t N = 10000;
    // Голый токен без разделителей: декорирование (`/name` против `Local\name`) — дело шва.
    const std::string name = "likenes_ide_" + std::to_string(platform::process_id());

    platform::SharedMemory::unlink(name);
    platform::SharedMemory m;
    if (!m.open(name, sizeof(MirrorBuffer), /*create=*/true, /*writable=*/false)) {
        std::printf("shmem create fail\n");
        return 3;
    }
    const auto* buf = static_cast<const MirrorBuffer*>(m.data());
    std::vector<MirrorEntity> snap(MIRROR_CAPACITY);
    uint32_t cnt = 0;

    // --- Гейт 3: spawn normal + read зеркало 10k ---
    platform::Child normal;
    check(spawn(normal, child, name, "normal", N), "gate3: editor spawns the game process");
    bool got = false;
    for (int i = 0; i < 3000 && !got; ++i) {
        got = (read_mirror(buf, snap, cnt) == ReadResult::Ok);
        if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(got, "gate3: read consistent mirror snapshot");
    check(got && cnt == N, "gate3: mirror has 10k entities");
    bool guids_ok = got;
    for (uint32_t i = 0; got && i < cnt; ++i)
        if (snap[i].guid != 1000 + i) { guids_ok = false; break; }
    check(guids_ok, "gate3: entity guids match expected pattern");
    bool gen_ok = got;
    uint32_t g0 = got ? snap[0].gen : 0;
    for (uint32_t i = 1; got && i < cnt; ++i)
        if (snap[i].gen != g0) { gen_ok = false; break; }
    check(gen_ok, "gate3: snapshot internally consistent (single generation)");
    // Останов игры редактором — штатный исход, и шов обязан отличать его от падения.
    check(normal.kill_and_wait(), "gate3: editor stops the running game (killed, not exited)");
    // Ребёнок пожат ровно один раз: повторный останов обязан вернуть false, а не «убить» заново
    // случайный процесс с тем же pid. Заодно это доказывает, что сироты не осталось.
    check(!normal.kill_and_wait(), "gate3: the stopped child is reaped exactly once");

    // --- Гейт 4: spawn crash → child падает сам, parent жив ---
    platform::Child crasher;
    check(spawn(crasher, child, name, "crash", N), "gate4: crash-mode child spawned");
    platform::ExitStatus st;
    check(crasher.wait(st), "gate4: editor collects the child's exit status");
    // Обычная сборка → нарушение доступа (null-deref): SIGSEGV на POSIX, код исключения на
    // Windows — оба исхода шов сводит к одному ExitKind::Crashed, в этом и смысл гейта.
    // Санитайзерный прогон гоняется с ASAN_OPTIONS=handle_segv=0 (см. шаг ASan в ci.yml): иначе
    // ASan перехватил бы интенциональный SEGV и вышел штатным exit(1) — артефакт инструментации,
    // неотличимый от чистого выхода.
    if (st.kind != platform::ExitKind::Crashed)
        std::printf("  (exit kind=%d code=%d)\n", static_cast<int>(st.kind), st.code);
    check(st.kind == platform::ExitKind::Crashed, "gate4: child died from a crash, not a clean exit");
    read_mirror(buf, snap, cnt);   // parent жив: чтение устаревшего зеркала не роняет редактор
    check(true, "gate4: parent survived child crash + read stale mirror");

    // --- Layout-drift (часть гейта 8): child пишет несовместимый layout → parent отвергает ---
    // NB: после гейта 4 в зеркале валидный stale-снапшот от crash-child (layout=1). Ждём, пока
    // badlayout-child перезапишет заголовок (layout=999) → reader обязан вернуть LayoutDrift.
    platform::Child bad;
    check(spawn(bad, child, name, "badlayout", N), "layout-drift: mismatched child spawned");
    ReadResult r = ReadResult::NotReady;
    for (int i = 0; i < 3000; ++i) {
        r = read_mirror(buf, snap, cnt);
        if (r == ReadResult::LayoutDrift) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(r == ReadResult::LayoutDrift, "layout-drift: reader rejects mismatched snapshot (no garbage read)");
    check(bad.kill_and_wait(), "layout-drift: mismatched child stopped");

    m.close();   // owner → unlink

    bool pass = (failures == 0);
    std::printf("ide-play-spawn: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
