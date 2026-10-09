#pragma once
#include <cstdint>
#include <vector>
#include "action_map.hpp"
#include "device_state.hpp"
#include "input_buffer.hpp"
#include "input_spsc.hpp"
#include "input_types.hpp"

// InputEngine: связывает input-поток (SPSC сырых событий) и sim-поток (дренаж @tick).
// drain коалесцирует события тика один раз, resolve даёт InputFrame каждого игрока. Гибрид:
// InputFrame-поток пишется в record (хешируется/реплеится); сырые события — debug-лог (не хешится).
namespace input {

constexpr int RAW_QUEUE_CAP = 4096; // степень двойки
constexpr int BUFFER_TICKS = 32;

class InputEngine {
public:
    explicit InputEngine(const ActionMap& map) : map_(map) {}

    // input-поток (продюсер): положить сырое событие ОС. При переполнении очереди событие
    // отбрасывается и считается в dropped() (диагностика: потерянный up/release → залипание).
    bool post(const RawEvent& e) { bool ok = queue_.push(e); if (!ok) ++dropped_; return ok; }
    uint64_t dropped() const { return dropped_; }

    // sim-поток (консюмер) @sync_point тика: дренаж и coalesce ОДИН раз на тик, затем resolve
    // каждого игрока. Дренаж на игрока раздал бы события тика первому, кто позвал, а латч
    // дельты мыши обнулил бы её для остальных.
    void drain() {
        RawEvent e;
        while (queue_.pop(e)) {
            device_.apply(e);
            if (record_events_) event_log_.push_back(e); // debug-лог, НЕ хешится
        }
        device_.latch_frame_delta();
    }

    // Async-дренаж: коалесцировать события ДО TickMark (продюсер помечает границу тика).
    // Возвращает false, если маркер не встретился (продюсер ещё не дошёл) — вызвать позже.
    bool drain_marked() {
        RawEvent e;
        while (queue_.pop(e)) {
            if (e.kind == RawKind::TickMark) { device_.latch_frame_delta(); return true; }
            device_.apply(e);
        }
        return false;
    }

    // prev_held свой у каждого игрока: общий отдал бы фронт pressed тому, кто разрешён первым, а
    // второй видел бы уровень чужого тика. Игрок вне [0, MAX_PLAYERS) не пишет ни в один слот.
    const InputFrame& resolve(uint32_t tick, int player) {
        if (slot(player) == NOBODY) return slots_[NOBODY].frame;
        PlayerSlot& s = slots_[player];
        s.frame = map_.resolve(device_, player, tick, s.prev_held);
        s.prev_held = s.frame.held;
        s.buffer.push(s.frame);
        if (recording_) s.record.push_back(s.frame);
        return s.frame;
    }

    // Одиночный путь игрока 0. Игрока параметром не берут: второй вызов за тик заново латчил бы
    // мышь в ноль, а begin_tick_marked съел бы маркер следующего тика. Хотсит — drain + resolve.
    const InputFrame& begin_tick(uint32_t tick) { drain(); return resolve(tick, 0); }

    bool begin_tick_marked(uint32_t tick) {
        if (!drain_marked()) return false;
        resolve(tick, 0);
        return true;
    }
    const InputFrame& frame(int player = 0) const { return slots_[slot(player)].frame; }

    // Реплей: подать записанный InputFrame напрямую (в обход устройств) — детерм. гейт.
    const InputFrame& replay_tick(const InputFrame& rec, int player = 0) {
        if (slot(player) == NOBODY) return slots_[NOBODY].frame;
        PlayerSlot& s = slots_[player];
        s.frame = rec;
        s.buffer.push(s.frame);
        return s.frame;
    }

    void set_recording(bool on) { recording_ = on; }
    void set_event_logging(bool on) { record_events_ = on; }
    const std::vector<InputFrame>& record(int player = 0) const { return slots_[slot(player)].record; }
    const std::vector<RawEvent>& event_log() const { return event_log_; }

    const InputBuffer<BUFFER_TICKS>& buffer(int player = 0) const { return slots_[slot(player)].buffer; }
    DeviceState& device() { return device_; }

private:
    struct PlayerSlot {
        InputFrame frame;
        uint64_t prev_held = 0;
        InputBuffer<BUFFER_TICKS> buffer;
        std::vector<InputFrame> record;
    };
    // Лишний слот «никто» никогда не пишется: игрок вне диапазона читает пустой кадр, а не
    // чужой слот и не память за массивом.
    static constexpr int NOBODY = MAX_PLAYERS;
    static int slot(int player) { return player >= 0 && player < MAX_PLAYERS ? player : NOBODY; }

    const ActionMap& map_;
    SpscQueue<RawEvent, RAW_QUEUE_CAP> queue_;
    DeviceState device_;
    PlayerSlot slots_[MAX_PLAYERS + 1];
    uint64_t dropped_ = 0;
    bool recording_ = false;
    bool record_events_ = false;
    std::vector<RawEvent> event_log_;
};

} // namespace input
