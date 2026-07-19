#include "platform_io.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

namespace asset {

MappedFile::~MappedFile() { close(); }

MappedFile::MappedFile(MappedFile&& o) noexcept : data_(o.data_), size_(o.size_), fd_(o.fd_) {
    o.data_ = nullptr;
    o.size_ = 0;
    o.fd_ = -1;
}

MappedFile& MappedFile::operator=(MappedFile&& o) noexcept {
    if (this != &o) {
        close();
        data_ = o.data_;
        size_ = o.size_;
        fd_ = o.fd_;
        o.data_ = nullptr;
        o.size_ = 0;
        o.fd_ = -1;
    }
    return *this;
}

bool MappedFile::open(const std::string& path) {
    close();
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) return false;
    struct stat st{};
    if (fstat(fd_, &st) != 0 || st.st_size <= 0) {
        close();
        return false;
    }
    size_ = static_cast<size_t>(st.st_size);
    void* p = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (p == MAP_FAILED) {
        close();
        return false;
    }
    data_ = static_cast<const uint8_t*>(p);
    return true;
}

void MappedFile::close() {
    if (data_) {
        munmap(const_cast<uint8_t*>(data_), size_);
        data_ = nullptr;
    }
    size_ = 0;
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

IoCaps MappedFile::caps() {
    return IoCaps{/*mmap=*/true, /*direct_stream=*/false};
}

bool read_range(const MappedFile& file, size_t offset, size_t len, uint8_t* dst) {
    // Проверка без переполнения: offset+len может обернуться.
    if (!file.valid() || len > file.size() || offset > file.size() - len) return false;
    std::memcpy(dst, file.data() + offset, len);
    return true;
}

} // namespace asset
