#pragma once
#include <cstdint>

#include "net_link.hpp"

// Инъектор транспорта для гейта 3 (спека #22): потери, дубли, перестановка и задержка —
// ДЕТЕРМИНИРОВАННЫЕ, от зерна. Живая сеть эти события тоже даёт, но красный прогон на ней нельзя
// отличить от невезения, а зелёный — от того, что событие не случилось ни разу.
//
// Он же — не «мок ради теста»: канал стоит ровно там, где в бою стоит сокет (Link отдаёт
// датаграммы в ящик и ничего не знает о носителе), поэтому проверяется НАСТОЯЩИЙ путь пакета, а
// не его подобие.
namespace net {

struct ChannelPolicy {
    uint32_t loss_percent = 0;
    uint32_t duplicate_percent = 0;
    uint32_t reorder_percent = 0;
    // На сколько тактов придерживается переставленный пакет. Ноль означает, что перестановки не
    // будет, сколько бы процентов ни стояло рядом.
    uint32_t reorder_delay = 0;
    uint32_t latency = 0;
};

class Channel {
public:
    Channel(uint32_t seed, const ChannelPolicy& policy) : rng_(seed), policy_(policy) {}

    void offer(const Datagram& d, uint32_t now) {
        ++offered_;
        if (roll() < policy_.loss_percent) {
            ++dropped_;
            return;
        }
        uint32_t due = now + policy_.latency;
        if (policy_.reorder_delay != 0 && roll() < policy_.reorder_percent) {
            due += policy_.reorder_delay;
            ++delayed_;
        }
        put(d, due);
        // Дубль — копия ТОЙ ЖЕ датаграммы с тем же сроком: сеть дублирует пакет, а не сообщение,
        // и подмена копии на повторную отправку проверяла бы переотправку, а не дедупликацию.
        if (roll() < policy_.duplicate_percent) {
            put(d, due);
            ++duplicated_;
        }
    }

    // Забрать созревшую датаграмму. Из созревших выбирается положенная РАНЬШЕ прочих: без этого
    // порядок зависел бы от номера слота, то есть перестановка была бы свойством хранилища.
    bool take(uint32_t now, Datagram* out) {
        size_t best = CAPACITY;
        for (size_t i = 0; i < CAPACITY; ++i) {
            if (!slots_[i].live || slots_[i].due > now) continue;
            if (best == CAPACITY || slots_[i].order < slots_[best].order) best = i;
        }
        if (best == CAPACITY) return false;
        *out = slots_[best].data;
        slots_[best].live = false;
        ++taken_;
        return true;
    }

    uint32_t offered() const { return offered_; }
    uint32_t dropped() const { return dropped_; }
    uint32_t duplicated() const { return duplicated_; }
    uint32_t delayed() const { return delayed_; }
    uint32_t taken() const { return taken_; }
    // Переполнение хранилища. Обязано быть нулём: канал, потерявший пакет от тесноты, измеряет
    // размер собственного буфера, а не поведение слоя над ним.
    uint32_t overflowed() const { return overflowed_; }

private:
    static constexpr size_t CAPACITY = 256;

    struct Slot {
        Datagram data;
        uint32_t due = 0;
        uint32_t order = 0;
        bool live = false;
    };

    // Линейный конгруэнтный, числа Numerical Recipes. Свой, а не <random>: реализации
    // стандартных распределений разнятся между libstdc++, libc++ и MSVC, и «тот же прогон на
    // трёх ОС» перестал бы быть тем же прогоном.
    uint32_t roll() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return (rng_ >> 16) % 100u;
    }

    void put(const Datagram& d, uint32_t due) {
        for (size_t i = 0; i < CAPACITY; ++i) {
            if (slots_[i].live) continue;
            slots_[i].data = d;
            slots_[i].due = due;
            slots_[i].order = order_++;
            slots_[i].live = true;
            return;
        }
        ++overflowed_;
    }

    Slot slots_[CAPACITY];
    uint32_t rng_ = 0;
    ChannelPolicy policy_;
    uint32_t order_ = 0;
    uint32_t offered_ = 0;
    uint32_t dropped_ = 0;
    uint32_t duplicated_ = 0;
    uint32_t delayed_ = 0;
    uint32_t taken_ = 0;
    uint32_t overflowed_ = 0;
};

} // namespace net
