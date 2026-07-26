#pragma once
#include <cstdint>
#include <vector>
#include "action_map.hpp"
#include "device_state.hpp"
#include "input_buffer.hpp"
#include "spsc.hpp"
#include "input_types.hpp"

// InputEngine: связывает input-поток (SPSC сырых событий) и sim-поток (дренаж @tick).
// begin_tick детерминированно коалесцирует события в InputFrame игрока. Гибрид:
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

    // sim-поток (консюмер) @sync_point тика: дренаж → coalesce → resolve → InputFrame.
    const InputFrame& begin_tick(uint32_t tick, int player = 0) {
        RawEvent e;
        while (queue_.pop(e)) {
            device_.apply(e);
            if (record_events_) event_log_.push_back(e); // debug-лог, НЕ хешится
        }
        device_.latch_frame_delta();
        frame_ = map_.resolve(device_, player, tick, prev_held_);
        prev_held_ = frame_.held;
        buffer_.push(frame_);
        if (recording_) record_.push_back(frame_);
        return frame_;
    }

    // Async-дренаж: коалесцировать события ДО TickMark (продюсер помечает границу тика).
    // Возвращает false, если маркер не встретился (продюсер ещё не дошёл) — вызвать позже.
    bool begin_tick_marked(uint32_t tick, int player = 0) {
        RawEvent e;
        while (queue_.pop(e)) {
            if (e.kind == RawKind::TickMark) {
                device_.latch_frame_delta();
                frame_ = map_.resolve(device_, player, tick, prev_held_);
                prev_held_ = frame_.held;
                buffer_.push(frame_);
                if (recording_) record_.push_back(frame_);
                return true;
            }
            device_.apply(e);
        }
        return false;
    }
    const InputFrame& frame() const { return frame_; }

    // Реплей: подать записанный InputFrame напрямую (в обход устройств) — детерм. гейт.
    const InputFrame& replay_tick(const InputFrame& rec) {
        frame_ = rec;
        buffer_.push(frame_);
        return frame_;
    }

    void set_recording(bool on) { recording_ = on; }
    void set_event_logging(bool on) { record_events_ = on; }
    const std::vector<InputFrame>& record() const { return record_; }
    const std::vector<RawEvent>& event_log() const { return event_log_; }

    const InputBuffer<BUFFER_TICKS>& buffer() const { return buffer_; }
    DeviceState& device() { return device_; }

private:
    const ActionMap& map_;
    SpscQueue<RawEvent, RAW_QUEUE_CAP> queue_;
    DeviceState device_;
    InputBuffer<BUFFER_TICKS> buffer_;
    InputFrame frame_;
    uint64_t prev_held_ = 0;
    uint64_t dropped_ = 0;
    bool recording_ = false;
    bool record_events_ = false;
    std::vector<InputFrame> record_;
    std::vector<RawEvent> event_log_;
};

} // namespace input
