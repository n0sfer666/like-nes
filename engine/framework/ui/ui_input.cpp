#include "ui_input.hpp"

#include <algorithm>
#include <cmath>

#include "ui_input_sources.hpp"
#include "font_format.hpp"

namespace framework::ui {
namespace {
enum Action { Up, Down, Left, Right, Accept, Back, Erase };
constexpr const char* ACTIONS[]{"up", "down", "left", "right", "accept", "back", "erase"};
bool scalar(uint32_t cp) { return graphics::font_codepoint_ok(cp); }
}
int UiInputChannel::Repeat::step(int wanted, uint32_t ms) {
    if (!wanted) { *this = {}; return 0; }
    if (wanted != direction) { direction = wanted; elapsed = 0; first = true; return wanted; }
    elapsed += std::min(ms, 1000u);
    if (elapsed < (first ? 350u : 90u)) return 0;
    elapsed = 0; first = false;
    return wanted;
}
bool UiInputChannel::init(const input::PresetTable& table) {
    ready_ = false; layout_ = {}; previous_held_ = blocked_ = 0; previous_keys_ = {}; x_ = {}; y_ = {}; erase_ = {};
    pending_cancel_ = false; axes_ = {-1, -1}; paired_shape_ = -1; blocked_axes_ = {};
    const int p = table.find_preset("ui");
    if (p < 0) return false;
    uint64_t bindings = 0;
    for (uint32_t a = 0; a < table.action_count(p); ++a) bindings += table.action_binding_count(p, a);
    if (bindings > 256) return false; // предел до выделения ActionLayout
    for (size_t i = 0; i < actions_.size(); ++i) {
        actions_[i] = table.find_action(p, ACTIONS[i]);
        if (actions_[i] < 0 || table.action_binding_count(p, actions_[i]) == 0) return false;
    }
    if (!table.bind(p, layout_)) return false;
    for (size_t i = 0; i < axes_.size(); ++i) {
        axes_[i] = table.find_axis(p, i ? "y" : "x");
        if (axes_[i] >= 0) shapes_[i] = table.axis_shape(p, axes_[i]);
    }
    for (size_t i = 0; i < axes_.size(); ++i) {
        if (axes_[i] < 0 || table.axis_pair(p, axes_[i]) == input::NO_PAIR) continue;
        if (axes_[1 - i] < 0 || table.axis_pair(p, axes_[i]) != static_cast<uint32_t>(axes_[1 - i])) return false;
        if (paired_shape_ < 0) paired_shape_ = static_cast<int>(i);
    }
    ready_ = true;
    return true;
}
UiInput UiInputChannel::read(const ::input::DeviceState& d, uint32_t ms, std::span<const UiPointer> pointers,
                           std::span<const uint32_t> text, UiTextSource text_source) {
    UiInput out;
    bool invalid = pointers.size() > UI_POINTERS || (text_source == UiTextSource::Native && text.size() > UI_CHARACTERS) ||
                   (text_source != UiTextSource::PhysicalLatin && text_source != UiTextSource::Native);
    for (size_t i = 0; i < std::min(pointers.size(), UI_POINTERS); ++i) {
        const auto& p = pointers[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.phase > UiPointerPhase::Cancel) invalid = true;
    }
    out.cancelled = pending_cancel_ || invalid || !ready_;
    uint32_t held = 0;
    if (ready_) for (const auto& binding : layout_.buttons())
        for (size_t a = 0; a < actions_.size(); ++a)
            if (binding.action == actions_[a] && detail::ui_down(binding.src, d)) held |= uint32_t{1} << a;
    if (out.cancelled) blocked_ |= held;
    else blocked_ &= held;
    held &= ~blocked_;
    bool negative[2]{bool(held & (1u << Left)), bool(held & (1u << Up))};
    bool positive[2]{bool(held & (1u << Right)), bool(held & (1u << Down))};
    bool source_active[2]{};
    if (ready_) for (int pad = -1; pad < ::input::MAX_DEVICES; ++pad) {
        int64_t raw[2]{};
        for (const auto& binding : layout_.axes())
            for (size_t a = 0; a < axes_.size(); ++a) {
                if (binding.axis != axes_[a]) continue;
                const auto pos = detail::ui_axis(binding.pos, d, pad), neg = detail::ui_axis(binding.neg, d, pad);
                if (out.cancelled || blocked_axes_[a]) for (const auto source : {pos, neg}) {
                    const auto& shape = shapes_[paired_shape_ >= 0 ? paired_shape_ : static_cast<int>(a)];
                    source_active[a] = source_active[a] || input::axial(source, shape).raw != 0;
                }
                raw[a] += int64_t{pos.raw} - neg.raw;
            }
        input::Vec2 value{fix32::from_raw(static_cast<int32_t>(std::clamp<int64_t>(raw[0], -fix32::ONE, fix32::ONE))),
                          fix32::from_raw(static_cast<int32_t>(std::clamp<int64_t>(raw[1], -fix32::ONE, fix32::ONE)))};
        if (paired_shape_ >= 0) value = input::radial(value, shapes_[paired_shape_]);
        else { value.x = input::axial(value.x, shapes_[0]); value.y = input::axial(value.y, shapes_[1]); }
        const fix32 values[]{value.x, value.y};
        for (size_t a = 0; a < axes_.size(); ++a) {
            negative[a] = negative[a] || values[a].raw <= -fix32::ONE / 2;
            positive[a] = positive[a] || values[a].raw >= fix32::ONE / 2;
        }
    }
    int wanted[2]{};
    for (size_t a = 0; a < axes_.size(); ++a) {
        // Не терять D+A или два противоположных axis bindings за их нулевой суммой.
        if (out.cancelled) blocked_axes_[a] = blocked_axes_[a] || source_active[a] || negative[a] || positive[a];
        else if (!source_active[a] && !negative[a] && !positive[a]) blocked_axes_[a] = false;
        if (!blocked_axes_[a]) wanted[a] = int(positive[a]) - int(negative[a]);
    }
    keyboard(d, out, !out.cancelled && text_source == UiTextSource::PhysicalLatin);
    if (out.cancelled) {
        x_ = {}; y_ = {}; erase_ = {}; out.character_count = 0;
    } else {
        out.x = static_cast<int8_t>(x_.step(wanted[0], ms));
        out.y = static_cast<int8_t>(y_.step(wanted[1], ms));
        out.accept = (held & ~previous_held_ & (1u << Accept)) != 0;
        out.back = (held & ~previous_held_ & (1u << Back)) != 0;
        out.erase = erase_.step((held & (1u << Erase)) != 0, ms) != 0;
        out.pointer_count = static_cast<uint32_t>(pointers.size());
        std::copy(pointers.begin(), pointers.end(), out.pointers.begin());
        if (text_source == UiTextSource::Native)
            for (const auto cp : text) if (scalar(cp)) out.characters[out.character_count++] = cp;
    }
    previous_held_ = held; pending_cancel_ = false;
    return out;
}
}
