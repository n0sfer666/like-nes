#pragma once
#include <cstddef>
#include <cstdint>

#include "net_ack_window.hpp"
#include "platform_net.hpp"

// Надёжный слой поверх UDP (спека #22, требование «подтверждения, переотправка, дедупликация,
// упорядочивание»). Канал к ОДНОМУ пиру.
//
// Транспорта внутри нет намеренно. `Link` кладёт готовые датаграммы в исходящий ящик, а кто их
// понесёт — сокет или инъектор потерь — знает вызывающий. Без этой границы гейт 3 (потери,
// дубли, перестановки) пришлось бы ставить на живой сети, то есть на недетерминированном
// стенде, и красный прогон нельзя было бы отличить от невезения.
//
// «Упорядочивание» здесь означает ДЕДУПЛИКАЦИЮ и знание порядка, а не выдачу по порядку:
// откатной модели нужна свежесть (решение 4 спеки), и придержать свежий пакет ради пропавшего
// старого — ровно тот head-of-line blocking, из-за которого отвергнут TCP. Пакет, пришедший
// не в свою очередь, доставляется НЕМЕДЛЕННО и считается счётчиком reordered.
namespace net {

// magic("LNET") + version + kind + seq + ack + ack_bits
constexpr size_t HEADER_BYTES = 4 + 1 + 1 + 2 + 2 + 4;
constexpr size_t MAX_PAYLOAD = platform::net::MAX_DATAGRAM - HEADER_BYTES;

// Окно неподтверждённых. Оно же — потолок отставания: пир, молчащий дольше, чем окно делённое на
// темп отправки, получает отказ в send(), а не растущую очередь. Отказ — заявленный исход
// (требование спеки «разрыв — заявленный исход, а не неопределённое поведение»).
constexpr size_t SEND_WINDOW = 32;

struct Datagram {
    uint8_t bytes[platform::net::MAX_DATAGRAM];
    size_t size = 0;
};

enum class Received {
    Foreign,   // не наш протокол, не та версия или обрезанная датаграмма — молча выброшена
    Duplicate, // этот seq уже принимался или ушёл за окно истории
    Ack,       // валидный пакет без нагрузки: несёт только подтверждения
    Delivered, // нагрузка записана наружу
};

class Link {
public:
    // resend_after — сколько тактов ждать подтверждения, прежде чем слать заново. Такт задаёт
    // вызывающий (у нас — тик симуляции): собственных часов у слоя нет, и не должно быть —
    // они сделали бы прогон гейта зависящим от стенного времени.
    explicit Link(uint32_t resend_after) : resend_after_(resend_after) {}

    // false — окно переполнено или длина выше MAX_PAYLOAD. Сообщение НЕ принято: тихо потерять
    // его здесь значило бы сделать «надёжный» слой ненадёжным ровно на переполнении.
    bool send(const void* data, size_t n);

    // Собрать исходящий ящик на такте `now`: новые сообщения, переотправки просроченных и —
    // если слать нечего, а подтверждения задолжали — один пакет без нагрузки. Молчащая сторона
    // иначе не подтверждает ничего, и у собеседника окно заполняется переотправками.
    void pump(uint32_t now);
    size_t outbox_size() const { return outbox_size_; }
    const Datagram& outbox_at(size_t i) const { return outbox_[i]; }

    // Разбор ЧУЖОЙ датаграммы: длина, magic и версия проверяются до единого чтения нагрузки.
    Received receive(const void* datagram, size_t n, void* payload, size_t cap, size_t* out_n);

    uint32_t sent() const { return sent_; }
    uint32_t resent() const { return resent_; }
    uint32_t delivered() const { return delivered_; }
    uint32_t duplicates() const { return duplicates_; }
    uint32_t foreign() const { return foreign_; }
    uint32_t reordered() const { return recv_.reordered(); }
    uint32_t refused() const { return refused_; }
    size_t unacked() const;

private:
    struct Pending {
        uint8_t data[MAX_PAYLOAD];
        size_t size = 0;
        uint16_t seq = 0;
        uint32_t sent_at = 0;
        bool live = false;
        bool fresh = false; // ещё ни разу не отправлено — отличает первую отправку от повтора
    };

    void emit(const Pending& p, bool repeat);
    void retire(uint16_t seq);
    void emit_ack();
    void ack_upto(uint16_t ack, uint32_t bits);

    Pending window_[SEND_WINDOW];
    Datagram outbox_[SEND_WINDOW + 1];
    size_t outbox_size_ = 0;

    uint32_t resend_after_ = 0;
    uint16_t next_seq_ = 0;

    AckWindow recv_;
    bool ack_owed_ = false;

    uint32_t sent_ = 0;
    uint32_t resent_ = 0;
    uint32_t delivered_ = 0;
    uint32_t duplicates_ = 0;
    uint32_t foreign_ = 0;
    uint32_t refused_ = 0;
};

} // namespace net
