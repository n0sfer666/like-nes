#include "pad_registry.hpp"

namespace framework::input {

void PadRegistry::connected(int slot, const ::input::PadInfo& info, const PresetTable& table) {
    if (slot < 0 || slot >= ::input::MAX_DEVICES) return;
    profiles_[slot] = table.profile_for(info);
    active_[slot] = true;
}

void PadRegistry::disconnected(int slot) {
    if (slot < 0 || slot >= ::input::MAX_DEVICES) return;
    // Сбрасывается ФЛАГ, а не запись: профиль отключённого слота недостижим — profile() отдаёт
    // generic по флагу, а подключение следующего пада перезаписывает запись целиком. Обнулять
    // её отдельно значило бы завести строку, которую ни один тест не в силах уронить.
    active_[slot] = false;
}

bool PadRegistry::active(int slot) const {
    return slot >= 0 && slot < ::input::MAX_DEVICES && active_[slot];
}

const PadProfile& PadRegistry::profile(int slot) const {
    if (!active(slot)) return generic_;
    return profiles_[slot];
}

} // namespace framework::input
