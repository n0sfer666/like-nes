#pragma once

#include "net_wire.hpp"
#include "platformer_scene.hpp"

// Что именно едет по сети в вертикали 1 (спека #22, решение 1): номер тика и нажатия одного
// игрока — и больше ничего. Мир не едет вовсе, и этот файл ровно об этом.
//
// Поле за полем: у `MoveInput` есть байт выравнивания, в который никто не пишет (его нашёл шаг A),
// и отправка структуры как есть означала бы, что подтверждение отличается от предсказания по
// содержимому набивки — то есть откат на каждом тике.
namespace platformer::input_wire {

constexpr size_t BYTES = 9;

inline uint8_t bits_of(const ch::MoveInput& in) {
    return static_cast<uint8_t>((in.jump_held ? 1u : 0u) | (in.down_held ? 2u : 0u) |
                                (in.up_held ? 4u : 0u));
}

// Возвращает `ok()` писателя, а не void: `BYTES` — константа рядом, и формат, доросший до десятого
// байта без её правки, уехал бы в сеть НЕИНИЦИАЛИЗИРОВАННЫМ хвостом буфера. Разошлись бы пиры
// именно по нему — то есть ровно по той набивке, ради которой этот файл и заведён.
inline bool put(uint8_t* body, uint32_t tick, const ch::MoveInput& in) {
    net::Writer w(body, BYTES);
    w.u32(tick);
    w.u32(static_cast<uint32_t>(in.move_x.raw));
    w.u8(bits_of(in));
    return w.ok();
}

inline bool get(const uint8_t* body, size_t n, uint32_t& tick, ch::MoveInput& in) {
    net::Reader r(body, n);
    uint32_t move_x = 0;
    uint8_t bits = 0;
    if (!r.u32(&tick) || !r.u32(&move_x) || !r.u8(&bits)) return false;
    in.move_x = fix32::from_raw(static_cast<int32_t>(move_x));
    in.jump_held = (bits & 1u) != 0;
    in.down_held = (bits & 2u) != 0;
    in.up_held = (bits & 4u) != 0;
    return true;
}

} // namespace platformer::input_wire
