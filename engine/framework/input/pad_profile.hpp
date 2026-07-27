#pragma once
#include <cstdint>

#include "source.hpp"
#include "stick.hpp"

// Профиль геймпада: форма отклика стиков и триггеров конкретной модели плюс набор надписей на
// её кнопках. Профиль — это НЕ перебинд: перебинд выбирает игрок и он живёт в сохранении, а
// профиль есть свойство железа и одинаков у всех владельцев этого пада.
//
// Коды кнопок профиль НЕ переставляет, и это не упущение. Все три бэкенда сообщают кнопку
// ПОЗИЦИЕЙ на ромбе (evdev BTN_SOUTH, XInput A, GameController buttonA — это одна и та же
// нижняя кнопка), поэтому «прыжок под большим пальцем» уже совпадает на Xbox и Switch. Меняются
// только НАДПИСИ: то, что на Xbox подписано «B», на Switch подписано «A». Отсюда labels —
// свойство подсказок в UI, а не трансляции ввода; перестановка кодов сломала бы ровно тот гейт,
// ради которого профили и заводились.
namespace framework::input {

enum class PadLabels : uint8_t {
    Xbox = 0,       // юг = A, восток = B
    Nintendo = 1,   // юг = B, восток = A
    Playstation = 2 // юг = ✕, восток = ○
};

struct PadProfile {
    const char* name = "generic";
    PadLabels labels = PadLabels::Xbox;
    StickShape stick;
    fix32 trigger_threshold = fix32::from_float(0.12);
};

// Надпись на кнопке по её позиционному коду — для подсказок «нажмите …».
const char* button_label(const PadProfile& p, uint16_t code);

} // namespace framework::input
