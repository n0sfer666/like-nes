#pragma once
// Читаемые коды. Клавиши — ASCII/GLFW-совместимо; pad — раскладка Xbox (индексы как
// в GLFW/SDL gamepad, на них же ложатся XInput и GameController.framework).
namespace input::code {

// Keys (подмножество, ASCII для печатных — совпадает с GLFW).
enum Key : int { Space = 32, A = 65, D = 68, S = 83, W = 87, Esc = 256, Enter = 257 };

enum Mouse : int { MLeft = 0, MRight = 1, MMiddle = 2 };
enum MouseAx : int { MAxX = 0, MAxY = 1, MAxWheel = 2 };

// Xbox-раскладка pad-кнопок.
enum Pad : int { PadA = 0, PadB = 1, PadX = 2, PadY = 3, LB = 4, RB = 5,
                 Back = 6, Start = 7, LStick = 9, RStick = 10,
                 DpUp = 11, DpRight = 12, DpDown = 13, DpLeft = 14 };

// ЗНАК ОСЕЙ — часть контракта, а не деталь бэкенда: +X вправо, +Y ВНИЗ (экранная система),
// триггеры [0, 1]. Направление держится тут, потому что три ОС расходятся ровно в нём: evdev
// отдаёт +Y вниз, а XInput и GameController — вверх, и бэкенд, пропустивший сырое значение
// наружу, инвертирует стик только на своей платформе. Пресеты собраны под это соглашение
// (`move_y` берёт `padaxis:-ly`), так что менять его — значит переснимать бейк и его эталоны.
enum PadAx : int { LX = 0, LY = 1, RX = 2, RY = 3, LT = 4, RT = 5 };

} // namespace input::code
