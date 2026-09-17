#pragma once
#include "sim.hpp"
#include <cstdint>
#include <string>

// Хозяин untrusted-плагина: WAT → песочница → тик гравитации. Песочница и её потолки живут в
// `wasm_sandbox.hpp` — их делит с зондом побега (`wasm_escape.hpp`), и потолок, поставленный
// только на игровом пути, оставил бы гейт мерить другую конфигурацию (аудит #21, ревью A·2).
class WasmGravity {
public:
    WasmGravity() = default;
    ~WasmGravity();
    WasmGravity(const WasmGravity&) = delete;
    WasmGravity& operator=(const WasmGravity&) = delete;

    bool init(const std::string& wat_path);
    // Тот же конструктор из текста WAT: гейту нужен плагин, которого нет файлом в дереве.
    bool init_wat(const std::string& wat);
    bool apply(SimWorld& w, int32_t g_raw, int32_t dt_raw);
    // Причина последнего отказа; на успехе пусто. Непустая строка после удачного захода врала бы
    // о живом объекте словами прошлой попытки (аудит #21, ревью A·2).
    const std::string& error() const { return err_; }
    // Расход ПОСЛЕДНЕГО вызова, включая трапнувший: гость, выпивший бак, называет весь бак, и
    // ровно этим «кадр съело топливо» отличимо от «гость упал на второй инструкции». Ноль здесь
    // значит «измерения нет»: бак не налился или счётчик отказался отвечать (ревью A·2).
    uint64_t fuel_used() const { return fuel_used_; }

private:
    struct Impl;
    // Пустая песочница под новую попытку: оба входа начинают с неё, поэтому неудачный `init()`
    // повторяем, а второй заход не бросает недостроенный Impl первого (аудит #21, ревью A·2).
    void reset_impl();
    bool init_module(const std::string& wat);
    void read_fuel();
    Impl* p_ = nullptr;
    bool ready_ = false;
    uint64_t fuel_used_ = 0;
    std::string err_;
};
