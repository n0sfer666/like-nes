#pragma once
#include "input_types.hpp"
#include "pad_profile.hpp"
#include "presets.hpp"

// Кто сейчас подключён и по какому профилю. Реестр держит профиль СЛОТА, а не устройства:
// слот — это то, чем игрок владеет с точки зрения игры, и переподключение того же пада в другой
// порт не должно выглядеть как смена игрока.
//
// Профиль ищется один раз на подключении, а не на каждом тике: поиск идёт по строкам таблицы и
// сравнению имён, и в тике ему делать нечего (гейт 7 — ноль аллокаций и ноль лишней работы).
namespace framework::input {

class PadRegistry {
public:
    void connected(int slot, const ::input::PadInfo& info, const PresetTable& table);
    void disconnected(int slot);

    bool active(int slot) const;
    const PadProfile& profile(int slot) const;

private:
    PadProfile profiles_[::input::MAX_DEVICES];
    bool active_[::input::MAX_DEVICES] = {};
    PadProfile generic_;
};

} // namespace framework::input
