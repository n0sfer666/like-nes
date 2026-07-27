#include "platform_shmem.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace platform {
namespace {

std::string decorate(const std::string& name) { return "/" + name; }

} // namespace

bool SharedMemory::open(const std::string& name, size_t size, bool create, bool writable) {
    if (addr_ != nullptr || size == 0 || !detail::shmem_name_ok(name)) return false;
    const std::string path = decorate(name);

    // Читатель открывает O_RDONLY → сам дескриптор read-only (defense-in-depth поверх PROT_READ).
    // Путь создания требует O_RDWR: ftruncate на read-only дескрипторе не проходит.
    const int access = writable ? O_RDWR : O_RDONLY;
    const int oflag = create ? (O_CREAT | O_EXCL | O_RDWR) : access;
    const int fd = shm_open(path.c_str(), oflag, 0600);
    if (fd < 0) return false;
    if (create && ftruncate(fd, static_cast<off_t>(size)) != 0) {
        ::close(fd);
        shm_unlink(path.c_str());
        return false;
    }
    // Присоединение к чужому сегменту сверяет фактический размер. Проверка несёт ЛИНУКС: там
    // mmap за пределы объекта проходит успешно, а платой становится SIGBUS на первом обращении.
    // Darwin завышенный запрос отвергает сам, Windows — тоже (MapViewOfFile), поэтому без этих
    // строк рассинхрон layout редактор↔игра ронял бы читателя ровно на одной ОС из трёх.
    if (!create) {
        struct stat st{};
        if (fstat(fd, &st) != 0 || static_cast<size_t>(st.st_size) < size) {
            ::close(fd);
            return false;
        }
    }

    const int prot = writable ? (PROT_READ | PROT_WRITE) : PROT_READ;
    void* addr = mmap(nullptr, size, prot, MAP_SHARED, fd, 0);
    // Дескриптор больше не нужен: маппинг держит собственную ссылку на объект (см. заголовок).
    ::close(fd);
    if (addr == MAP_FAILED) {
        if (create) shm_unlink(path.c_str());
        return false;
    }

    addr_ = addr;
    size_ = size;
    name_ = name;
    owner_ = create;
    writable_ = writable;
    return true;
}

void SharedMemory::close() {
    if (addr_ != nullptr) {
        munmap(addr_, size_);
        addr_ = nullptr;
    }
    if (owner_ && !name_.empty()) shm_unlink(decorate(name_).c_str());
    size_ = 0;
    name_.clear();
    owner_ = false;
    writable_ = false;
}

void SharedMemory::unlink(const std::string& name) {
    if (!detail::shmem_name_ok(name)) return;
    shm_unlink(decorate(name).c_str());
}

} // namespace platform
