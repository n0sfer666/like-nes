#pragma once
#include <array>
#include <cstdint>
#include <span>

#include "device_state.hpp"
#include "presets.hpp"

namespace framework::ui {
enum class UiPointerPhase : uint8_t { Down, Move, Up, Cancel };
struct UiPointer {
    intptr_t id = 0;
    UiPointerPhase phase = UiPointerPhase::Cancel;
    float x = 0, y = 0; // пиксели зоны, до масштаба viewport
};
inline constexpr size_t UI_POINTERS = 16, UI_CHARACTERS = 64;
enum class UiTextSource : uint8_t { PhysicalLatin, Native };
struct UiInput {
    int8_t x = 0, y = 0; // шаг навигации; удержание повторяется через350/90ms
    bool accept = false, back = false, erase = false, cancelled = false;
    std::array<UiPointer, UI_POINTERS> pointers{};
    std::array<uint32_t, UI_CHARACTERS> characters{};
    uint32_t pointer_count = 0, character_count = 0;
};

// Канал читает уже дренированное состояние, не трогает очередь, PlayerAssign или игровые edges.
// init вне кадра; read/cancel не выделяют память. PresetTable не требуется после init.
class UiInputChannel {
public:
    bool init(const input::PresetTable& table);
    UiInput read(const ::input::DeviceState& devices, uint32_t elapsed_ms,
                 std::span<const UiPointer> pointers = {}, std::span<const uint32_t> text = {},
                 UiTextSource text_source = UiTextSource::PhysicalLatin);
    // После cancel удержанные кнопки/оси должны отпуститься, прежде чем снова дать ввод.
    void cancel() { pending_cancel_ = true; }
private:
    struct Repeat {
        int direction = 0;
        uint32_t elapsed = 0;
        bool first = true;
        int step(int wanted, uint32_t ms);
    };
    void keyboard(const ::input::DeviceState& devices, UiInput& out, bool emit);
    ::input::ActionLayout layout_;
    std::array<int, 7> actions_{};
    std::array<int, 2> axes_{{-1, -1}};
    std::array<input::StickShape, 2> shapes_{};
    int paired_shape_ = -1;
    std::array<uint64_t, ::input::KBD_KEYS / 64> previous_keys_{};
    std::array<bool, 2> blocked_axes_{};
    Repeat x_, y_, erase_;
    uint32_t previous_held_ = 0, blocked_ = 0;
    bool ready_ = false, pending_cancel_ = false;
};
}
