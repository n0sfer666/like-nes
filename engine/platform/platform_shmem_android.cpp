#include "platform_shmem.hpp"

// Android: shm_open в bionic нет — именованная память там живёт в ashmem/ASharedMemory, и имени
// в общем пространстве системы у неё нет вовсе. Потребители шва — редактор и его дочерняя игра
// (спека #13) — на мобиле не запускаются, поэтому здесь явный отказ: open всегда false.
namespace platform {

bool SharedMemory::open(const std::string&, size_t, bool, bool) {
    close();
    return false;
}

void SharedMemory::close() {
    addr_ = nullptr;
    size_ = 0;
    name_.clear();
    native_ = 0;
    owner_ = false;
    writable_ = false;
}

void SharedMemory::unlink(const std::string&) {}

} // namespace platform
