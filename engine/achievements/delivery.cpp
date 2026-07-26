#include "delivery.hpp"

namespace ach {
namespace {

constexpr uint32_t OP_UNLOCK = 0;
constexpr uint32_t OP_STAT = 1;
constexpr int32_t REMOTE_CAP = 64;
constexpr std::size_t COMPACT_THRESHOLD = 64;

} // namespace

Delivery::Delivery(const Registry& reg, Tracker& tracker, Backend& backend)
    : reg_(reg), tracker_(tracker), backend_(backend) {
    mirrored_.assign(tracker_.stat_ids().size(), 0);
    delivered_.assign(tracker_.ach_ids().size(), 0);
}

Delivery::~Delivery() { shutdown(); }

// Контракт begin/end симметричен: штатное завершение обязано закрыть сессию бэкенда,
// иначе платформенный адаптер (SteamAPI_Shutdown) не выключается никогда.
void Delivery::shutdown() {
    if (closed_) return;
    closed_ = true;
    if (stats_.dead || !stats_.connected) return;
    if (dirty_) backend_.commit();
    backend_.end();
    stats_.connected = false;
}

void Delivery::collect() {
    const std::vector<Id>& ach = tracker_.ach_ids();
    const std::vector<uint8_t>& flags = tracker_.ach_flags();
    for (std::size_t i = 0; i < ach.size(); ++i) {
        if (flags[i] == 0 || delivered_[i] != 0) continue;
        delivered_[i] = 1;
        queue_.push_back(Op{OP_UNLOCK, ach[i], 0});
    }
    const std::vector<Id>& stat_ids = tracker_.stat_ids();
    const std::vector<uint64_t>& values = tracker_.stat_values();
    for (std::size_t i = 0; i < stat_ids.size(); ++i) {
        if (values[i] == mirrored_[i]) continue;
        mirrored_[i] = values[i];
        queue_stat(stat_ids[i], values[i]);
    }
}

// Стат — это последнее значение, а не событие: пока бэкенд отвечает Retry, неотправленная запись
// обновляется на месте. Иначе очередь растёт неограниченно и в платформу льётся устаревший хвост.
void Delivery::queue_stat(Id id, uint64_t value) {
    for (std::size_t i = head_; i < queue_.size(); ++i) {
        if (queue_[i].kind != OP_STAT || queue_[i].id != id) continue;
        queue_[i].value = value;
        return;
    }
    queue_.push_back(Op{OP_STAT, id, value});
}

Send Delivery::send(const Op& op) {
    if (op.kind == OP_UNLOCK) {
        const Entry* e = reg_.find(op.id);
        if (e == nullptr) return Send::Ok;
        return backend_.unlock(e->key);
    }
    const Stat* s = reg_.find_stat(op.id);
    if (s == nullptr) return Send::Ok;
    return backend_.set_stat(s->key, op.value);
}

void Delivery::fail() {
    stats_.dropped += queue_.size() - head_;
    queue_.clear();
    head_ = 0;
    dirty_ = false;
    stats_.dead = true;
    stats_.connected = false;
    backend_.end();
}

void Delivery::compact() {
    if (head_ < COMPACT_THRESHOLD || head_ * 2 < queue_.size()) return;
    queue_.erase(queue_.begin(), queue_.begin() + static_cast<std::ptrdiff_t>(head_));
    head_ = 0;
}

void Delivery::pump(uint32_t max_ops) {
    if (stats_.dead || closed_) return;
    if (!stats_.connected) {
        if (!backend_.begin()) return;
        stats_.connected = true;
        for (const Entry& e : reg_.entries()) backend_.declare(e.key);
    }
    collect();

    for (uint32_t n = 0; n < max_ops && head_ < queue_.size(); ++n) {
        const Send r = send(queue_[head_]);
        if (r == Send::Retry) {
            ++stats_.retried;
            break;
        }
        if (r == Send::Fatal) {
            fail();
            return;
        }
        ++stats_.sent;
        ++head_;
        dirty_ = true;
    }

    if (dirty_) {
        const Send c = backend_.commit();
        if (c == Send::Fatal) {
            fail();
            return;
        }
        if (c == Send::Ok) {
            ++stats_.commits;
            dirty_ = false;
        } else {
            ++stats_.retried;
        }
    }
    compact();
}

void Delivery::reconcile() {
    if (stats_.dead || closed_ || !stats_.connected) return;
    const char* keys[REMOTE_CAP] = {nullptr};
    const int32_t n = backend_.poll_remote(keys, REMOTE_CAP);
    if (n <= 0) return;

    const std::vector<Id>& ach = tracker_.ach_ids();
    for (int32_t i = 0; i < n && i < REMOTE_CAP; ++i) {
        if (keys[i] == nullptr) continue;
        const Id id = hash_key(keys[i]);
        if (reg_.find(id) == nullptr) continue;
        for (std::size_t slot = 0; slot < ach.size(); ++slot) {
            if (ach[slot] != id) continue;
            if (tracker_.ach_flags()[slot] == 0) {
                tracker_.unlock(id);
                ++stats_.reconciled;
            }
            delivered_[slot] = 1;
            break;
        }
    }
}

} // namespace ach
