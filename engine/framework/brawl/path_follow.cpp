#include "path_follow.hpp"

#include <cstdio>
#include <cstdlib>

#include "fixmath.hpp"

namespace framework::brawl {

namespace {

uint8_t next_vertex(const scene::Path& path, const PathFollow& f) {
    if (path.loop) return static_cast<uint8_t>((f.segment + 1u) % path.count);
    return static_cast<uint8_t>(f.segment + f.direction);
}

void next_segment(const scene::Path& path, PathFollow& f) {
    f.segment = next_vertex(path, f);
    if (path.loop) return;
    if (f.segment == path.count - 1u) f.direction = -1;
    if (f.segment == 0) f.direction = 1;
}

fix32 segment_length(const scene::Path& path, const PathFollow& f) {
    return length(path.points[next_vertex(path, f)] - path.points[f.segment]);
}

// Длина полного цикла: петля — периметр, маятник — путь туда и обратно. Цикл возвращает следование
// в то же состояние вместе с направлением, поэтому целые циклы хода можно отбросить, не сдвинув точку.
uint32_t cycle_segments(const scene::Path& path) { return path.loop ? path.count : 2u * (path.count - 1u); }

fix32 cycle_length(const scene::Path& path) {
    fix32 sum;
    PathFollow f;
    for (uint32_t i = 0; i < cycle_segments(path); ++i) {
        sum = sum + segment_length(path, f);
        next_segment(path, f);
    }
    return sum;
}

} // namespace

// Ход берётся по модулю цикла: точку это не сдвигает, а число шагов ниже ограничивает одним циклом,
// а не отношением скорости к длине пути — до 32 px за тик на пути в доли пикселя. Путь нулевой длины
// стоит. Остаток меньше цикла, значит шагов не больше сегментов цикла; дошли до конца — следование
// рассогласовано с длиной цикла, и abort громче, чем бесконечный цикл, вешающий игру.
void advance_path(const scene::Path& path, PathFollow& f) {
    if (path.count < 2 || !(fix32{} < path.speed)) return;
    const fix32 cycle = cycle_length(path);
    if (cycle.raw == 0) return;
    fix32 left = fix32::from_raw((f.traveled + path.speed).raw % cycle.raw);
    for (uint32_t i = 0; i <= cycle_segments(path); ++i) {
        const fix32 len = segment_length(path, f);
        if (left < len) {
            f.traveled = left;
            return;
        }
        left = left - len;
        next_segment(path, f);
    }
    std::fprintf(stderr, "[brawl] path follow ran a whole cycle without stopping: segments disagree with the cycle\n");
    std::abort();
}

Vec2 path_point(const scene::Path& path, const PathFollow& f) {
    const Vec2 from = path.points[f.segment];
    if (path.count < 2) return from;
    const Vec2 span = path.points[next_vertex(path, f)] - from;
    const fix32 len = length(span);
    if (len.raw == 0) return from;
    return from + span * (f.traveled / len);
}

} // namespace framework::brawl
