#pragma once

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

#include <vector>

#include "art.hpp"
#include "batch.hpp"
#include "gpu.hpp"
#include "platformer_view.hpp"

// Окно образца-платформера: GLFW, поверхность, батч и показ ОДНОГО состояния сцены.
//
// Отдельным файлом от прогона, потому что прогонов у окна стало ДВА — одиночный (гейт 8 спеки #16)
// и сетевой (гейт 9 спеки #22). Вторая копия этих ста строк разошлась бы с первой молча: настройка
// поверхности, распаковка тона и порядок отрисовки не проверяются ни одним гейтом, и расхождение
// увидел бы только человек, у которого одна из двух целей рисует не то.
//
// Утверждать про этот файл на раннере по-прежнему нечего — он тонкий нарочно: всё, что можно
// проверить headless, живёт в `platformer_view.hpp` и проверяется `game_platformer_view_test`.
namespace platformer {

class Window {
public:
    // `false` — окна или устройства нет. Причина печатается здесь же: вызывающему остаётся только
    // выйти, и второй раз объяснять её было бы нечем.
    bool open(const char* title);
    void close();

    GLFWwindow* handle() const { return win_; }

    // Разбор очереди событий ОС. Отдельно от показа, потому что ввод читается между ними: движок
    // ввода спрашивают уже после того, как события кадра разобраны.
    void poll();

    // Человек закрыл окно крестиком или нажал Esc. Спрашивается ПОСЛЕ `poll`, иначе ответ описывал
    // бы предыдущий кадр.
    bool quit_asked() const;

    // Кадр по состоянию сцены. Поверхность на `Fifo`, поэтому вызов ждёт вертикальной синхронизации
    // и служит тактом сессии — и одиночному прогону, и сетевому.
    void draw(const Stage& stage);

private:
    GLFWwindow* win_ = nullptr;
    GpuContext gpu_;
    WGPUSurface surface_ = nullptr;
    WGPUTextureFormat fmt_ = WGPUTextureFormat_Undefined;
    uint32_t fbw_ = 0;
    uint32_t fbh_ = 0;
    game::Atlas atlas_;
    game::SpriteBatch batch_;
    std::vector<Quad> quads_;
    FrameSprites sprites_;
    bool surface_warned_ = false;
};

} // namespace platformer
