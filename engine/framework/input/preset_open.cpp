#include "presets.hpp"

#include <cstring>

namespace framework::input {
namespace {

template <typename T>
const T* view(const uint8_t* base, uint32_t offset, uint32_t count, std::size_t size) {
    // Границы — единственная защита zero-parse формата: дальше по нему ходят указателями. Верхнего
    // предела мало: смещение проверяется снизу (иначе строки лягут поверх заголовка) и на
    // выравнивание (`reinterpret_cast` мимо границы = SIGBUS, как в `asset/bundle_view.cpp`), а
    // сумма считается в uint64 — в `size_t` на 32 битах `count * sizeof(T)` заворачивается.
    if (offset < sizeof(PresetHeader) || offset % alignof(T) != 0) return nullptr;
    if (static_cast<uint64_t>(offset) + static_cast<uint64_t>(count) * sizeof(T) > size)
        return nullptr;
    return reinterpret_cast<const T*>(base + offset);
}

} // namespace

bool PresetTable::open(const void* data, std::size_t size) {
    header_ = nullptr;
    if (data == nullptr || size < sizeof(PresetHeader)) return false;
    const auto* base = static_cast<const uint8_t*>(data);
    const auto* h = reinterpret_cast<const PresetHeader*>(base);
    if (std::memcmp(h->magic, PRESET_MAGIC, sizeof(h->magic)) != 0) return false;
    if (h->version != PRESET_VERSION || h->total_size > size) return false;

    presets_ = view<PresetRow>(base, h->presets_offset, h->preset_count, size);
    actions_ = view<ActionRow>(base, h->actions_offset, h->action_count, size);
    axes_ = view<AxisRow>(base, h->axes_offset, h->axis_count, size);
    bindings_ = view<BindingRow>(base, h->bindings_offset, h->binding_count, size);
    pads_ = view<PadRow>(base, h->pads_offset, h->pad_count, size);
    if (presets_ == nullptr || actions_ == nullptr || axes_ == nullptr || bindings_ == nullptr ||
        pads_ == nullptr)
        return false;
    if (h->strings_offset >= h->total_size) return false;

    strings_ = reinterpret_cast<const char*>(base + h->strings_offset);
    strings_size_ = h->total_size - h->strings_offset;
    if (strings_[strings_size_ - 1] != '\0') return false;   // обход имён обязан упереться в ноль
    header_ = h;
    return true;
}

} // namespace framework::input
