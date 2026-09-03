#pragma once
#include <cstdint>

// Скользящее окно принятых номеров: дедупликация входящих и материал для поля `ack_bits`
// исходящих. Отдельной сущностью, а не парой полей внутри Link, по двум причинам: у неё своя
// арифметика (переполнение uint16_t, сдвиг на ширину типа) и свой предмет проверки — «второй
// раз тот же номер не проходит», который к транспорту отношения не имеет вовсе.
namespace net {

// Глубина истории: `ack_bits` — 32 бита, шире окно быть не может. Подтвердить можно только то,
// что помнишь принятым.
constexpr uint16_t ACK_HISTORY = 32;

// Свежесть номера с учётом переполнения uint16_t. Без половины диапазона в условии номер 1
// оказался бы «старше» номера 65535 навсегда, и на первом же обороте счётчика связь вставала бы
// намертво — классика, которую дешевле написать один раз, чем ловить на 65536-м пакете.
inline bool more_recent(uint16_t a, uint16_t b) {
    constexpr uint16_t half = 32768;
    return ((a > b) && (a - b <= half)) || ((b > a) && (b - a > half));
}

class AckWindow {
public:
    // true — номер принимается впервые. false — уже принимался ЛИБО ушёл за глубину истории:
    // отличить одно от другого нечем, и выбор в пользу дубля намеренный — доставить второй раз
    // хуже, чем не доставить старьё.
    bool note(uint16_t seq) {
        if (!valid_) {
            valid_ = true;
            latest_ = seq;
            bits_ = 0;
            return true;
        }
        if (more_recent(seq, latest_)) {
            const uint16_t delta = static_cast<uint16_t>(seq - latest_);
            // Сдвиг на ширину типа — UB, а не ноль, поэтому граница разобрана отдельной ветвью,
            // а не «сдвинем и посмотрим». Бит delta-1 — место, куда съезжает прежний последний.
            if (delta > ACK_HISTORY) {
                bits_ = 0;
            } else if (delta == ACK_HISTORY) {
                bits_ = 1u << (ACK_HISTORY - 1);
            } else {
                bits_ = (bits_ << delta) | (1u << (delta - 1));
            }
            latest_ = seq;
            return true;
        }
        if (seq == latest_) return false;
        const uint16_t back = static_cast<uint16_t>(latest_ - seq);
        if (back > ACK_HISTORY) return false;
        const uint32_t bit = 1u << (back - 1);
        if ((bits_ & bit) != 0) return false;
        bits_ |= bit;
        ++reordered_;
        return true;
    }

    bool valid() const { return valid_; }
    uint16_t latest() const { return latest_; }
    uint32_t bits() const { return bits_; }
    // Сколько номеров пришло ПОЗЖЕ более свежего. Не статистика: гейт 3 обязан доказать, что
    // перестановка в прогоне действительно случалась, иначе он зелен вакуумно.
    uint32_t reordered() const { return reordered_; }

private:
    bool valid_ = false;
    uint16_t latest_ = 0;
    uint32_t bits_ = 0;
    uint32_t reordered_ = 0;
};

} // namespace net
