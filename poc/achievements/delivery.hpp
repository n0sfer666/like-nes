#pragma once
#include <cstddef>
#include <vector>

#include "backend.hpp"
#include "tracker.hpp"

namespace ach {

struct DeliveryStats {
    uint64_t sent = 0;
    uint64_t retried = 0;
    uint64_t dropped = 0;
    uint64_t reconciled = 0;
    uint64_t commits = 0;
    bool connected = false;
    bool dead = false;
};

// Доставка наружу (Steam и подобные) — ВНЕ тика: очередь + ретраи + сверка.
// Sim и Tracker про неё не знают; отвал бэкенда деградирует молча (локальный прогресс — истина).
class Delivery {
public:
    Delivery(const Registry& reg, Tracker& tracker, Backend& backend);
    ~Delivery();
    Delivery(const Delivery&) = delete;
    Delivery& operator=(const Delivery&) = delete;

    void pump(uint32_t max_ops = 16);
    void reconcile();

    // Владелец ОБЯЗАН позвать shutdown() до выгрузки плагина, давшего backend: после dlclose
    // вызов end() уходит в размапленный код. Идемпотентно, деструктор — последний рубеж.
    void shutdown();

    std::size_t pending() const { return queue_.size() - head_; }
    const DeliveryStats& stats() const { return stats_; }

private:
    struct Op {
        uint32_t kind;
        Id id;
        uint64_t value;
    };

    void collect();
    void queue_stat(Id id, uint64_t value);
    Send send(const Op& op);
    void fail();
    void compact();

    const Registry& reg_;
    Tracker& tracker_;
    Backend& backend_;

    std::vector<Op> queue_;
    std::size_t head_ = 0;
    std::vector<uint64_t> mirrored_;
    std::vector<uint8_t> delivered_;
    bool dirty_ = false;
    bool closed_ = false;
    DeliveryStats stats_;
};

} // namespace ach
