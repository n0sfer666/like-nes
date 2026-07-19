#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Ассет-I/O за platform-частью HAL (спека #1 open Q #2 / спека #5). Desktop-нативно:
// mmap для резидента (zero-copy) + pread для async-стрима. Console/mobile backend — позже,
// в те же точки (open/map/read/close + caps).
namespace asset {

struct IoCaps {
    bool mmap = false;
    bool direct_stream = false; // io_uring/DirectStorage — desktop PoC: false (pread-стайл)
};

// RAII-обёртка mmap'нутого файла. data() валиден до close/деструктора (zero-copy база бандла).
class MappedFile {
public:
    MappedFile() = default;
    ~MappedFile();
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& o) noexcept;            // для транзакционного reload
    MappedFile& operator=(MappedFile&& o) noexcept;

    bool open(const std::string& path);
    void close();

    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }
    bool valid() const { return data_ != nullptr; }

    static IoCaps caps();

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    int fd_ = -1;
};

// Синхронное чтение диапазона (async-стрим-путь эмулирует это на worker-потоке).
// Копирует [offset, offset+len) из mmap-базы в dst (транзит через арену вызывающего).
bool read_range(const MappedFile& file, size_t offset, size_t len, uint8_t* dst);

} // namespace asset
