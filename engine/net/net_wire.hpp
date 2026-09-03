#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Проводной формат надёжного слоя (спека #22). Байты пишутся и читаются ПОЛЕ ЗА ПОЛЕМ в
// little-endian, а не копированием структуры: у любой структуры с разнородными полями есть
// набивка, её содержимое не определено, и «одинаковый пакет» на двух машинах перестал бы им
// быть. Тот же довод, что заставил кольцо ввода отката сравнивать поля вместо memcmp.
//
// Порядок байт задан ЯВНО и не совпадает с сетевым нарочно: big-endian у сокетов — соглашение
// про заголовки IP, а не про полезную нагрузку, и обе стороны здесь наши.
//
// Читатель обязан быть тотальным: на входе НЕДОВЕРЕННАЯ датаграмма из сети, обрезанная,
// склеенная или сочинённая. Каждое чтение проверяет остаток, и первый же выход за границу
// переводит читателя в негодное состояние навсегда — «прочитали половину и пошли дальше» есть
// худший из возможных исходов разбора чужого ввода.
namespace net {

class Writer {
public:
    Writer(void* buffer, size_t capacity)
        : buf_(static_cast<uint8_t*>(buffer)), cap_(buffer == nullptr ? 0 : capacity) {}

    bool u8(uint8_t v) { return raw(&v, 1); }
    bool u16(uint16_t v) {
        const uint8_t b[2] = {static_cast<uint8_t>(v & 0xFFu), static_cast<uint8_t>(v >> 8)};
        return raw(b, 2);
    }
    bool u32(uint32_t v) {
        const uint8_t b[4] = {static_cast<uint8_t>(v & 0xFFu),
                              static_cast<uint8_t>((v >> 8) & 0xFFu),
                              static_cast<uint8_t>((v >> 16) & 0xFFu),
                              static_cast<uint8_t>((v >> 24) & 0xFFu)};
        return raw(b, 4);
    }
    bool bytes(const void* data, size_t n) { return raw(static_cast<const uint8_t*>(data), n); }

    size_t size() const { return size_; }
    // Отказ ЛИПКИЙ: одна не поместившаяся запись портит весь пакет, и отправлять его нельзя,
    // сколько бы полей ни записалось после. Проверять каждое возвращаемое значение вызывающий
    // при этом не обязан — достаточно одного вопроса в конце.
    bool ok() const { return ok_; }

private:
    bool raw(const uint8_t* src, size_t n) {
        if (!ok_ || n > cap_ - size_) {
            ok_ = false;
            return false;
        }
        std::memcpy(buf_ + size_, src, n);
        size_ += n;
        return true;
    }

    uint8_t* buf_ = nullptr;
    size_t cap_ = 0;
    size_t size_ = 0;
    bool ok_ = true;
};

class Reader {
public:
    Reader(const void* buffer, size_t size)
        : buf_(static_cast<const uint8_t*>(buffer)), size_(buffer == nullptr ? 0 : size) {}

    bool u8(uint8_t* out) { return raw(out, 1); }
    bool u16(uint16_t* out) {
        uint8_t b[2];
        if (!raw(b, 2)) return false;
        *out = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
        return true;
    }
    bool u32(uint32_t* out) {
        uint8_t b[4];
        if (!raw(b, 4)) return false;
        *out = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
               (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
        return true;
    }
    bool bytes(void* out, size_t n) { return raw(static_cast<uint8_t*>(out), n); }

    size_t remaining() const { return ok_ ? size_ - at_ : 0; }
    bool ok() const { return ok_; }

private:
    bool raw(uint8_t* dst, size_t n) {
        if (!ok_ || n > size_ - at_) {
            ok_ = false;
            return false;
        }
        std::memcpy(dst, buf_ + at_, n);
        at_ += n;
        return true;
    }

    const uint8_t* buf_ = nullptr;
    size_t size_ = 0;
    size_t at_ = 0;
    bool ok_ = true;
};

} // namespace net
