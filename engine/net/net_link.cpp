#include "net_link.hpp"

#include <cstring>

#include "net_wire.hpp"

namespace net {
namespace {

// 'L','N','E','T' — не украшение: на порт прилетает и чужой трафик, и старая версия нас самих,
// и первый же разбор мусора как заголовка стоил бы окна дедупликации.
constexpr uint32_t MAGIC = 0x4C4E4554u;
constexpr uint8_t VERSION = 1;

// `kind` — битовое поле, а не перечисление: пакет бывает с нагрузкой, с подтверждениями и с тем
// и другим. Ноль и любой неизвестный бит — чужой пакет: неизвестный бит означает несовпадение
// версий, а не «можно проигнорировать».
constexpr uint8_t KIND_PAYLOAD = 1;
constexpr uint8_t KIND_ACK = 2;
constexpr uint8_t KIND_KNOWN = KIND_PAYLOAD | KIND_ACK;

} // namespace

bool Link::send(const void* data, size_t n) {
    if (data == nullptr || n == 0 || n > MAX_PAYLOAD) {
        ++refused_;
        return false;
    }
    // Слот выбирается номером, а не поиском свободного: так порядок слотов совпадает с порядком
    // номеров, и pump обходит окно от самого старого. Занятый слот означает ровно то, что окно
    // полно — пир не подтверждает.
    Pending& p = window_[static_cast<size_t>(next_seq_) % SEND_WINDOW];
    if (p.live) {
        ++refused_;
        return false;
    }
    std::memcpy(p.data, data, n);
    p.size = n;
    p.seq = next_seq_;
    p.live = true;
    p.fresh = true;
    p.sent_at = 0;
    ++next_seq_;
    return true;
}

void Link::pump(uint32_t now) {
    outbox_size_ = 0;
    // Обход начинается со слота следующего номера — это самый СТАРЫЙ живой слот, потому что
    // окно шириной ровно SEND_WINDOW и номера в нём подряд. Свежие уходят последними.
    const size_t base = static_cast<size_t>(next_seq_) % SEND_WINDOW;
    for (size_t i = 0; i < SEND_WINDOW; ++i) {
        Pending& p = window_[(base + i) % SEND_WINDOW];
        if (!p.live) continue;
        if (p.fresh) {
            p.fresh = false;
            p.sent_at = now;
            emit(p, false);
        } else if (now - p.sent_at >= resend_after_) {
            p.sent_at = now;
            emit(p, true);
        }
    }
    // Пакет без нагрузки шлётся, только когда слать больше нечего: подтверждения ездят на
    // данных, и молчащая сторона иначе не подтвердила бы ничего — окно собеседника заполнилось
    // бы переотправками того, что давно доставлено.
    if (outbox_size_ == 0 && ack_owed_) emit_ack();
}

void Link::emit(const Pending& p, bool repeat) {
    if (outbox_size_ >= SEND_WINDOW + 1) return;
    Datagram& d = outbox_[outbox_size_];
    Writer w(d.bytes, sizeof(d.bytes));
    w.u32(MAGIC);
    w.u8(VERSION);
    w.u8(static_cast<uint8_t>(KIND_PAYLOAD | (recv_.valid() ? KIND_ACK : 0)));
    w.u16(p.seq);
    w.u16(recv_.latest());
    w.u32(recv_.bits());
    w.bytes(p.data, p.size);
    if (!w.ok()) return;
    d.size = w.size();
    ++outbox_size_;
    if (repeat) {
        ++resent_;
    } else {
        ++sent_;
    }
    ack_owed_ = false;
}

void Link::emit_ack() {
    Datagram& d = outbox_[outbox_size_];
    Writer w(d.bytes, sizeof(d.bytes));
    w.u32(MAGIC);
    w.u8(VERSION);
    w.u8(KIND_ACK);
    // Номер в пакете без нагрузки не расходуется: он не доставляется и дедупликации не подлежит.
    // Тратить на него seq значило бы дырявить окно собеседника номерами, которых он не увидит.
    w.u16(0);
    w.u16(recv_.latest());
    w.u32(recv_.bits());
    if (!w.ok()) return;
    d.size = w.size();
    ++outbox_size_;
    ack_owed_ = false;
}

Received Link::receive(const void* datagram, size_t n, void* payload, size_t cap, size_t* out_n) {
    Reader r(datagram, n);
    uint32_t magic = 0;
    uint8_t version = 0;
    uint8_t kind = 0;
    uint16_t seq = 0;
    uint16_t ack = 0;
    uint32_t bits = 0;
    const bool head = r.u32(&magic) && r.u8(&version) && r.u8(&kind) && r.u16(&seq) &&
                      r.u16(&ack) && r.u32(&bits);
    if (!head || magic != MAGIC || version != VERSION || kind == 0 || (kind & ~KIND_KNOWN) != 0) {
        ++foreign_;
        return Received::Foreign;
    }
    if ((kind & KIND_ACK) != 0) ack_upto(ack, bits);
    if ((kind & KIND_PAYLOAD) == 0) {
        // Хвост у пакета без нагрузки — не «лишние байты, которые можно отбросить», а признак
        // того, что разбор разошёлся с отправителем.
        if (r.remaining() != 0) {
            ++foreign_;
            return Received::Foreign;
        }
        return Received::Ack;
    }
    const size_t len = r.remaining();
    if (len == 0 || len > cap || payload == nullptr || out_n == nullptr) {
        ++foreign_;
        return Received::Foreign;
    }
    // Дедупликация ДО копирования наружу: повтор не имеет права переписать буфер вызывающего.
    if (!recv_.note(seq)) {
        ++duplicates_;
        return Received::Duplicate;
    }
    r.bytes(payload, len);
    *out_n = len;
    ++delivered_;
    ack_owed_ = true;
    return Received::Delivered;
}

void Link::retire(uint16_t seq) {
    Pending& p = window_[static_cast<size_t>(seq) % SEND_WINDOW];
    // Сверка номера обязательна: слот переиспользуется каждые SEND_WINDOW номеров, и
    // подтверждение старого номера иначе гасило бы чужое, ещё не доставленное сообщение.
    if (p.live && p.seq == seq) p.live = false;
}

void Link::ack_upto(uint16_t ack, uint32_t bits) {
    retire(ack);
    for (uint16_t i = 0; i < ACK_HISTORY; ++i) {
        if ((bits & (1u << i)) != 0) retire(static_cast<uint16_t>(ack - 1 - i));
    }
}

size_t Link::unacked() const {
    size_t n = 0;
    for (const Pending& p : window_) {
        if (p.live) ++n;
    }
    return n;
}

} // namespace net
