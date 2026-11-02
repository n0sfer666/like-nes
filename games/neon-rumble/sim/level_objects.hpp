#pragma once
#include <optional>
#include <string_view>
#include <vector>

#include "scene_objects.hpp"

namespace framework::scene {

enum class SectionMode : uint8_t { Belt, Platform };
struct Section {
    Area area;
    SectionMode mode = SectionMode::Belt;
};

struct Arena {
    Area area;
    int32_t id = 0;
};

struct WaveSpawn {
    Mark mark;
    int32_t arena = 0;
    int32_t wave = 0;
    std::string_view kind;
    int32_t players = 1;
};

struct Checkpoint {
    Mark mark;
    int32_t order = 0;
};

// Потолок модуля скорости ленты, px за тик: в platform лента переносит бойца атомарно и без свипа,
// проверяя только точку назначения, а такой перенос проскакивает стену, когда сдвиг больше её толщины
// и ширины корпуса вместе. Полтайла 16 px — запас под узкий корпус у тонкой стены, а не точная
// граница. Знак — направление по `x`; ноль загрузчик отказывает, как у пути.
inline constexpr int32_t CONVEYOR_SPEED_MAX = 8;

struct Conveyor {
    Area area;
    fix32 speed;
};

// Пределы длины троса, px. Снизу — корпус бойца: трос короче него держал бы седока ногами у опоры.
// Сверху — скорость: с 90° трос разгоняет седока до `sqrt(2gL)`, и на 256 px при тяготении падения
// образца это ~19 px за тик — свип корпуса за тик, а не прыжок сквозь тайл.
inline constexpr int32_t ROPE_LENGTH_MIN = 16;
inline constexpr int32_t ROPE_LENGTH_MAX = 256;
// Тросов на уровень: состояние тросов лежит в массиве, а не в векторе, чтобы снапшот отката копировал
// его без аллокации. Восемь — с запасом под прототип; уровень с лишним тросом загрузчик отвергает.
inline constexpr uint32_t ROPES_MAX = 8;

struct Rope {
    Mark mark;
    fix32 length;
};

struct LevelObjects {
    std::vector<Section> sections;
    std::vector<Area> depth_bands;
    std::vector<Area> walls;
    std::vector<Arena> arenas;
    std::vector<Mark> players;
    std::vector<WaveSpawn> waves;
    std::vector<Checkpoint> checkpoints;
    std::vector<Area> kills;
    std::vector<Conveyor> conveyors;
    std::vector<Path> paths;
    std::vector<Autoscroll> autoscrolls;
    std::vector<Rope> ropes;
    std::optional<Area> bounds;
};

} // namespace framework::scene
