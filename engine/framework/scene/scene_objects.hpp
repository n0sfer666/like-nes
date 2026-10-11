#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "fixmath.hpp"
#include "object_format.hpp"

namespace framework::scene {

struct LevelRect {
    fix32 x0, y0, x1, y1;
};

enum class SectionMode : uint8_t { Belt, Platform };
enum class ScrollAxis : uint8_t { X, Y };

struct Area {
    const tilemap::MapObject* object = nullptr;
    LevelRect rect{};
};

struct Mark {
    const tilemap::MapObject* object = nullptr;
    Vec2 at{};
};

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

struct Conveyor {
    Area area;
    fix32 speed;
};

struct Path {
    const tilemap::MapObject* object = nullptr;
    std::array<Vec2, tilemap::MAX_POLYGON_VERTICES> points{};
    uint8_t count = 0;
    fix32 speed;
    bool loop = false;
};

struct Autoscroll {
    Area area;
    ScrollAxis axis = ScrollAxis::X;
    fix32 speed;
};

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
