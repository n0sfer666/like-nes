#pragma once
#include <cstddef>
#include <string>

// POSIX shared-memory обёртка. Writer (game) создаёт RW; reader (editor) мапит PROT_READ —
// read-only НА УРОВНЕ ОС: инспекция физически не может писать в sim → детерминизм/реплей целы.
// owner делает shm_unlink при close (авто-cleanup). Windows — follow-up (код+CI-build).
namespace ide::ipc {

struct Shmem {
    void* addr = nullptr;
    size_t size = 0;
    int fd = -1;
    std::string name;
    bool owner = false;
};

// create=true: shm_open O_CREAT|O_EXCL + ftruncate (owner, cleanup при close).
// write_map=true: PROT_READ|PROT_WRITE (writer); иначе PROT_READ (read-only reader).
bool shmem_open(Shmem& m, const std::string& name, size_t size, bool create, bool write_map);
void shmem_close(Shmem& m);
void shmem_unlink(const std::string& name);   // форс-очистка осиротевшего сегмента

} // namespace ide::ipc
