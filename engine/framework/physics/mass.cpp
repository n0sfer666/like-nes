#include "mass.hpp"

namespace framework::physics {
namespace {

// Модуль берётся в int64, а не на месте в int32: `-INT32_MIN` в int32 не представим, то есть это
// знаковое переполнение, то есть UB, — а сюда приходит и сырая форма, до клампов `sanitize`.
int64_t max_abs_raw(const Shape& s) {
    int64_t m = 0;
    for (uint8_t i = 0; i < s.count; ++i) {
        const int64_t x = s.points[i].x.raw < 0 ? -static_cast<int64_t>(s.points[i].x.raw)
                                                : s.points[i].x.raw;
        const int64_t y = s.points[i].y.raw < 0 ? -static_cast<int64_t>(s.points[i].y.raw)
                                                : s.points[i].y.raw;
        if (x > m) m = x;
        if (y > m) m = y;
    }
    return m;
}

// Сдвиг, приводящий координаты ядра к величине не больше 2^bits. Интегралы площади и инерции —
// это суммы ПРОИЗВЕДЕНИЙ координат, и на полном размахе Q16.16 (4096 юнитов, то есть 2.7e8 raw)
// произведение трёх сомножителей уходит за int64 задолго до конца суммы. Масштаб степенью двойки,
// а не делением на размах: сдвиг точен в обе стороны, а деление внесло бы округление, зависящее от
// формы, — то есть момент инерции, чуть-чуть разный у одинаковых по смыслу тел.
int32_t shift_for(int64_t max_abs, int32_t bits) {
    int32_t sh = 0;
    while ((max_abs >> sh) > (int64_t{1} << bits)) ++sh;
    return sh;
}

} // namespace

Vec2 centroid(const Shape& s) {
    if (s.count == 1) return s.points[0];
    if (s.count == 2) {
        // Середина отрезка. Сумма делится на два сдвигом, а не умножением на 0.5: у нечётного raw
        // деление даёт floor, и это одно правило на оба знака — в отличие от усечения к нулю.
        return {fix32::from_raw((s.points[0].x.raw + s.points[1].x.raw) >> 1),
                fix32::from_raw((s.points[0].y.raw + s.points[1].y.raw) >> 1)};
    }

    const int32_t sh = shift_for(max_abs_raw(s), 16);
    int64_t num_x = 0, num_y = 0, den = 0;
    for (uint8_t i = 0; i < s.count; ++i) {
        const uint8_t j = static_cast<uint8_t>((i + 1) % s.count);
        const int64_t xi = s.points[i].x.raw >> sh, yi = s.points[i].y.raw >> sh;
        const int64_t xj = s.points[j].x.raw >> sh, yj = s.points[j].y.raw >> sh;
        const int64_t c = xi * yj - xj * yi;
        den += c;
        num_x += (xi + xj) * c;
        num_y += (yi + yj) * c;
    }
    if (den == 0) {
        // Нулевая площадь — вырожденное ядро, до которого не добралась выпуклая оболочка (её
        // зовёт `sanitize`, а сюда могут прийти и сырой формой). Среднее вершин — не «правильный»
        // центроид, но определённая точка внутри ядра, и это лучше, чем деление на ноль.
        int64_t sx = 0, sy = 0;
        for (uint8_t i = 0; i < s.count; ++i) {
            sx += s.points[i].x.raw;
            sy += s.points[i].y.raw;
        }
        return {fix32::from_raw(fix32::sat(sx / s.count)), fix32::from_raw(fix32::sat(sy / s.count))};
    }
    return {fix32::from_raw(fix32::sat((num_x / (3 * den)) << sh)),
            fix32::from_raw(fix32::sat((num_y / (3 * den)) << sh))};
}

int64_t unit_inertia_raw(const Shape& s) {
    const int64_t r = s.radius.raw;
    if (s.count == 1) {
        // Диск: I/m = r^2 / 2. Точная формула, а не приближение, — у круга нет причин быть
        // приблизительным, и именно он чаще всего оказывается снарядом, чью раскрутку видно.
        return ((r * r) >> fix32::SHIFT) / 2;
    }
    if (s.count == 2) {
        const Vec2 d = s.points[1] - s.points[0];
        const int64_t len = length(d).raw;
        const int64_t w = len + 2 * r;   // длина капсулы вместе с полусферами
        const int64_t h = 2 * r;
        // ПРИБЛИЖЕНИЕ, названное вслух: капсула считается по своему габаритному прямоугольнику.
        // Точная формула требует момента полудиска, а он содержит 4/(3*pi) — иррациональную
        // константу, которую в Q16.16 пришлось бы округлять, и округление это вошло бы в sim-хеш
        // навсегда. Габарит завышает момент, то есть капсула раскручивается чуть неохотнее
        // настоящей: заметно только в сравнении с другим движком, и это осознанная цена.
        return (((w * w) >> fix32::SHIFT) + ((h * h) >> fix32::SHIFT)) / 12;
    }

    const int32_t sh = shift_for(max_abs_raw(s), 12);
    int64_t num = 0, den = 0;
    for (uint8_t i = 0; i < s.count; ++i) {
        const uint8_t j = static_cast<uint8_t>((i + 1) % s.count);
        const int64_t xi = s.points[i].x.raw >> sh, yi = s.points[i].y.raw >> sh;
        const int64_t xj = s.points[j].x.raw >> sh, yj = s.points[j].y.raw >> sh;
        const int64_t c = xi * yj - xj * yi;
        num += c * (xi * xi + yi * yi + xi * xj + yi * yj + xj * xj + yj * yj);
        den += c;
    }
    if (den == 0) return 0;
    // Относительно начала координат — и сразу же переносится на центроид теоремой Гюйгенса—
    // Штейнера. Без переноса момент формы, заданной со смещением, был бы завышен на квадрат этого
    // смещения, и тело раскручивалось бы тем неохотнее, чем дальше автор отодвинул её вершины от
    // нуля, — то есть физика зависела бы от выбора начала координат в редакторе.
    const int64_t about_origin = ((num / (6 * den)) << (2 * sh)) >> fix32::SHIFT;
    const Vec2 c = centroid(s);
    const int64_t offset_sq =
        ((static_cast<int64_t>(c.x.raw) * c.x.raw + static_cast<int64_t>(c.y.raw) * c.y.raw)
         >> fix32::SHIFT);
    // Скруглением многоугольника (радиус при трёх и более вершинах) момент НЕ дополняется: такую
    // форму не собирает ни один конструктор, а собранная руками получит момент своего ядра —
    // заниженный не больше чем на радиус. Названо, чтобы не выглядело забытым.
    const int64_t about_centroid = about_origin - offset_sq;
    return about_centroid > 0 ? about_centroid : 0;
}

} // namespace framework::physics
