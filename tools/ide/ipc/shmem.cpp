#include "shmem.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace ide::ipc {

bool shmem_open(Shmem& m, const std::string& name, size_t size, bool create, bool write_map) {
    // reader (write_map=false) открывает O_RDONLY → fd тоже read-only (defense-in-depth поверх
    // mmap PROT_READ). create-path требует O_RDWR для ftruncate.
    int access = write_map ? O_RDWR : O_RDONLY;
    int oflag = create ? (O_CREAT | O_EXCL | O_RDWR) : access;
    int fd = shm_open(name.c_str(), oflag, 0600);
    if (fd < 0) return false;
    // FD_CLOEXEC через fcntl (портируемо; macOS shm_open не принимает O_CLOEXEC во флаге) →
    // shmem-fd не течёт через fork+exec в дочерний game-процесс.
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (create && ftruncate(fd, static_cast<off_t>(size)) != 0) {
        ::close(fd);
        shm_unlink(name.c_str());
        return false;
    }
    int prot = write_map ? (PROT_READ | PROT_WRITE) : PROT_READ;
    void* addr = mmap(nullptr, size, prot, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        ::close(fd);
        if (create) shm_unlink(name.c_str());
        return false;
    }
    m.addr = addr;
    m.size = size;
    m.fd = fd;
    m.name = name;
    m.owner = create;
    return true;
}

void shmem_close(Shmem& m) {
    if (m.addr) { munmap(m.addr, m.size); m.addr = nullptr; }
    if (m.fd >= 0) { ::close(m.fd); m.fd = -1; }
    if (m.owner && !m.name.empty()) { shm_unlink(m.name.c_str()); m.owner = false; }
}

void shmem_unlink(const std::string& name) {
    shm_unlink(name.c_str());
}

} // namespace ide::ipc
