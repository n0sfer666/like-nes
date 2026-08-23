#include "grid.hpp"

namespace framework::tilemap {
namespace {

// Целочисленное деление с округлением ВНИЗ. `fix32::operator/` тут не годится дважды: он усекает к
// нулю (левее начала координат окно уехало бы на тайл) и округляет частное до 1/65536, а координата
// тайла обязана быть точной — она индекс, а не расстояние.
int64_t floor_div(int64_t num, int64_t den) {
    int64_t q = num / den;
    if (num % den != 0 && (num < 0) != (den < 0)) --q;
    return q;
}

int32_t clamp_index(int64_t v, int64_t limit) {
    if (v < 0) return 0;
    if (limit < v) return static_cast<int32_t>(limit);
    return static_cast<int32_t>(v);
}

} // namespace

TileGrid::TileGrid(Vec2 origin, fix32 tile_size, uint32_t width, uint32_t height)
    : origin_(origin),
      // Размер тайла приводится к ЧЁТНОМУ числу raw и минимуму в два. Ноль — это деление на ноль
      // в координате тайла, то есть карта, схлопнувшаяся в одну ячейку. Чётность же несущая:
      // запрос разворачивает тайл в форму «центр плюс полуразмер», и при нечётном raw полуразмер
      // округляется вниз — между соседними тайлами остаётся щель в один raw, то есть пол из тайлов
      // перестаёт быть сплошным. Отказаться тут нечем (конструктор без исключений), поэтому вход
      // приводится, а не принимается на веру, — тот же приём, что `sanitize` у формы.
      tile_size_(fix32::from_raw(tile_size.raw < 2 ? 2 : (tile_size.raw & ~1))),
      width_(width),
      height_(height),
      flags_(static_cast<size_t>(width) * height, TILE_EMPTY) {}

// Запись ЗА КАРТОЙ — не ошибка вызывающего, а часть контракта: генератор уровня кладёт
// прямоугольники, и требовать от него отсекать их по краю значило бы просить каждого вызывающего
// написать отсечение заново. Молчаливым это не делает: `at` за картой отвечает пустотой, то есть
// записанное за краем не «потерялось», его там никогда и не было.
void TileGrid::set(uint32_t x, uint32_t y, TileFlags flags) {
    if (width_ <= x || height_ <= y) return;
    flags_[static_cast<size_t>(y) * width_ + x] = flags;
}

void TileGrid::fill(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, TileFlags flags) {
    for (uint32_t y = y0; y < y1; ++y) {
        for (uint32_t x = x0; x < x1; ++x) set(x, y, flags);
    }
}

TileFlags TileGrid::at(int32_t x, int32_t y) const {
    if (x < 0 || y < 0) return TILE_EMPTY;
    if (static_cast<uint32_t>(x) >= width_ || static_cast<uint32_t>(y) >= height_) return TILE_EMPTY;
    return flags_[static_cast<size_t>(y) * width_ + static_cast<uint32_t>(x)];
}

physics::Aabb TileGrid::tile_bounds(int32_t x, int32_t y) const {
    const Vec2 lo{origin_.x + tile_size_ * fix32::from_int(x),
                  origin_.y + tile_size_ * fix32::from_int(y)};
    return {lo, {lo.x + tile_size_, lo.y + tile_size_}};
}

TileWindow TileGrid::window(const physics::Aabb& probe) const {
    const int64_t ts = tile_size_.raw;
    const int64_t lo_x = floor_div(static_cast<int64_t>(probe.min.x.raw) - origin_.x.raw, ts);
    const int64_t lo_y = floor_div(static_cast<int64_t>(probe.min.y.raw) - origin_.y.raw, ts);
    // Правый край ВКЛЮЧАЕТСЯ: коробка, кончающаяся ровно на границе тайла, обязана этот тайл
    // видеть — иначе зонд, поставленный впритык к стене, стены не находит. Лишний тайл в окне
    // стоит одной проверки геометрии, потерянный — прохода сквозь стену.
    const int64_t hi_x = floor_div(static_cast<int64_t>(probe.max.x.raw) - origin_.x.raw, ts) + 1;
    const int64_t hi_y = floor_div(static_cast<int64_t>(probe.max.y.raw) - origin_.y.raw, ts) + 1;
    return {clamp_index(lo_x, width_), clamp_index(lo_y, height_), clamp_index(hi_x, width_),
            clamp_index(hi_y, height_)};
}

} // namespace framework::tilemap
