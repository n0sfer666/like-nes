#pragma once
#include "engine.hpp"
#include "pad_registry.hpp"
#include "presets.hpp"
#include "rebind_store.hpp"
#include "source.hpp"

// Отчёт probe'а: всё, что он рассказывает человеку за консолью. Отделено от управления, потому
// что доказательством гейта 8 служит именно ЭТОТ текст — его копируют в отчёт о прогоне, и
// формат должен меняться независимо от того, какими клавишами probe управляют.
namespace framework::input {

void report_bindings(const PresetTable& table, uint32_t preset, const RebindStore& store);

// Разница подключений с прошлого тика: паспорт устройства и выбранный по нему профиль. prev —
// массив на MAX_DEVICES, probe владеет им сам.
void report_pads(::input::InputEngine& engine, ::input::GamepadSource* pad, PadRegistry& reg,
                 const PresetTable& table, bool* prev);

void report_status(const PresetTable& table, uint32_t preset, const ::input::InputFrame& frame,
                   const PadRegistry& reg, uint32_t tick);

} // namespace framework::input
