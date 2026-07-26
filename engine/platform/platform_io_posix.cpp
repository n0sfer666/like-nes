#include "platform_io.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace platform {

bool MappedFile::open(const std::string& utf8_path) {
    close();
    const int fd = ::open(utf8_path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    struct stat st{};
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        ::close(fd);
        return false;
    }
    const size_t size = static_cast<size_t>(st.st_size);
    void* p = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    // mmap держит собственную ссылку на файл (POSIX): дескриптор больше не нужен ни для
    // валидности отображения, ни для close() — закрываем сразу, в том числе на ошибке.
    ::close(fd);
    if (p == MAP_FAILED) return false;
    data_ = static_cast<const uint8_t*>(p);
    size_ = size;
    return true;
}

void MappedFile::close() {
    if (data_) munmap(const_cast<uint8_t*>(data_), size_);
    data_ = nullptr;
    size_ = 0;
}

IoCaps MappedFile::caps() {
    return IoCaps{/*mmap=*/true, /*direct_stream=*/false};
}

} // namespace platform
