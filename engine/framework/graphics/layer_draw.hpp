#pragma once
#include <cstdint>
#include <span>

#include "camera.hpp"
#include "graphics_sprite.hpp"
#include "viewport.hpp"
#include "visual_read.hpp"

// Визуальный слой `LNVL` → спрайты (спека #24, В5б). Спрайты выходят в ЭКРАННЫХ пикселях: начало
// слоя привязано `world_to_screen_snapped`, шаг клетки — размер тайла на масштаб вида, поэтому при
// целом масштабе каждая клетка встаёт на пиксельную сетку и швов между тайлами нет.
//
// Мировая единица — пиксель карты Tiled: коллизии печатаются с `tile_size = tilewidth` (В5а), и
// камера, едущая за персонажем, обязана жить в тех же числах, что и декор.
namespace framework::graphics {

struct LayerFrame {
    Viewport view{};
    Camera camera{};
    CameraConfig config{};
    uint64_t tick = 0;
    // Номер материала спрайта — позиция guid текстуры в этом списке; бэкенд держит текстуры в том же
    // порядке. Чужой guid не рисуется и СЧИТАЕТСЯ.
    std::span<const uint64_t> textures;
};

struct LayerDrawStats {
    uint32_t visited = 0;
    uint32_t emitted = 0;
    uint32_t unknown = 0;
    // Слой не дорисован: список спрайтов полон (в нём же учтён отброшенный) или повтор по оси с
    // клеткой уже пикселя экрана — окно повтора бесконечно мелкое, и обход его не кончился бы.
    bool truncated = false;
};

// Регион тайлового спрайта — визуальный индекс уже с кадром анимации, флип — биты клетки. Регион
// image-слоя — ноль: «вся текстура». Пустая клетка (индекс 0) не рисуется, поэтому у тайлового
// спрайта регион ноль не встречается и два смысла не пересекаются.
LayerDrawStats draw_layer(SpriteList& out, const tilemap::VisualMap& map,
                          const tilemap::VisualLayer& layer, const LayerFrame& frame, int16_t order);

} // namespace framework::graphics
