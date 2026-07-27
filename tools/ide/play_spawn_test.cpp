#include "ipc/mirror.hpp"
#include "ipc/seqlock.hpp"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "platform_shmem.hpp"

// Гейты 3 (Play-spawn + read-only зеркало 10k), 4 (крэш-изоляция), layout-drift (часть гейта 8).
// Редактор = parent: создаёт shmem (owner, RO-мап), spawn'ит game_child (fork+exec), читает
// консистентный снапшот через seqlock. Крэш child не трогает parent. Layout-drift → reject.
using namespace ide::ipc;

namespace {
int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("  FAIL: %s\n", w); ++failures; } }

pid_t spawn(const char* child_path, const std::string& name, const char* mode, uint32_t count) {
    pid_t pid = fork();
    if (pid == 0) {
        std::string cs = std::to_string(count);
        execl(child_path, child_path, name.c_str(), mode, cs.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    return pid;
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
    if (argc < 2) { std::printf("usage: play_spawn_test <game_child_path>\n"); return 2; }
    const char* child = argv[1];
    const uint32_t N = 10000;
    // Голый токен без разделителей: декорирование (`/name` против `Local\name`) — дело шва.
    const std::string name = "likenes_ide_" + std::to_string(getpid());

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
    pid_t pid = spawn(child, name, "normal", N);
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
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);

    // --- Гейт 4: spawn crash → child умирает от сигнала, parent жив ---
    pid_t cpid = spawn(child, name, "crash", N);
    int cst = 0;
    waitpid(cpid, &cst, 0);
    check(WIFSIGNALED(cst), "gate4: crashed child terminated by signal");
    // Обычная сборка → SIGSEGV (null-deref). Под ASan sanitizer перехватывает SEGV и завершает
    // через abort() → SIGABRT (артефакт инструментации, как ASan×dlclose в #6). Оба доказывают
    // «child крэшнул, parent жив» (граница процессов).
    bool crash_sig = WIFSIGNALED(cst) && (WTERMSIG(cst) == SIGSEGV || WTERMSIG(cst) == SIGABRT);
    check(crash_sig, "gate4: child died from crash signal (SEGV, or ABRT under ASan)");
    read_mirror(buf, snap, cnt);   // parent жив: чтение устаревшего зеркала не роняет редактор
    check(true, "gate4: parent survived child crash + read stale mirror");

    // --- Layout-drift (часть гейта 8): child пишет несовместимый layout → parent отвергает ---
    // NB: после гейта 4 в зеркале валидный stale-снапшот от crash-child (layout=1). Ждём, пока
    // badlayout-child перезапишет заголовок (layout=999) → reader обязан вернуть LayoutDrift.
    pid_t bpid = spawn(child, name, "badlayout", N);
    ReadResult r = ReadResult::NotReady;
    for (int i = 0; i < 3000; ++i) {
        r = read_mirror(buf, snap, cnt);
        if (r == ReadResult::LayoutDrift) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(r == ReadResult::LayoutDrift, "layout-drift: reader rejects mismatched snapshot (no garbage read)");
    kill(bpid, SIGKILL);
    waitpid(bpid, nullptr, 0);

    m.close();   // owner → unlink

    bool pass = (failures == 0);
    std::printf("ide-play-spawn: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
