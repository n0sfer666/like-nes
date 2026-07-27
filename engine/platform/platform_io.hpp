#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Шов №1 платформы (спека #12, решение 2): маппинг файла. Реализации — platform_io_posix.cpp
// (mmap) и platform_io_win32.cpp (CreateFileMapping/MapViewOfFile), выбор делает CMake, поэтому
// условной компиляции нет ни здесь, ни в реализациях.
//
// Путь на входе — ВСЕГДА UTF-8: движок внутри работает в UTF-8, конвертация в UTF-16 живёт
// только в win32-реализации.
//
// Хендлов в объекте нет намеренно: и mmap, и секция Windows держат собственную ссылку на файл,
// поэтому дескриптор/HANDLE закрываются сразу после маппинга. Живым остаётся только view —
// его и освобождает close(). Меньше хендлов → нечему течь (гейт 3).
namespace platform {

struct IoCaps {
    bool mmap = false;
    bool direct_stream = false; // io_uring/DirectStorage — desktop PoC: false (pread-стайл)
};

// RAII-обёртка замапленного файла. data() валиден до close/деструктора (zero-copy база бандла).
class MappedFile {
public:
    MappedFile() = default;
    ~MappedFile() { close(); }
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    // Перемещение платформы не касается (только два поля) → inline, обе реализации его разделяют.
    MappedFile(MappedFile&& o) noexcept : data_(o.data_), size_(o.size_) {
        o.data_ = nullptr;
        o.size_ = 0;
    }
    MappedFile& operator=(MappedFile&& o) noexcept { // для транзакционного reload
        if (this != &o) {
            close();
            data_ = o.data_;
            size_ = o.size_;
            o.data_ = nullptr;
            o.size_ = 0;
        }
        return *this;
    }

    bool open(const std::string& utf8_path);
    void close();

    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }
    bool valid() const { return data_ != nullptr; }

    static IoCaps caps();

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
};

// Синхронное чтение диапазона (async-стрим-путь эмулирует это на worker-потоке).
// Копирует [offset, offset+len) из базы маппинга в dst (транзит через арену вызывающего).
// Платформо-независимо → inline, отдельного TU не заводим.
inline bool read_range(const MappedFile& file, size_t offset, size_t len, uint8_t* dst) {
    // Проверка без переполнения: offset+len может обернуться.
    if (!file.valid() || len > file.size() || offset > file.size() - len) return false;
    std::memcpy(dst, file.data() + offset, len);
    return true;
}

} // namespace platform
