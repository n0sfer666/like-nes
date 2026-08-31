#include <cmath>
#include <cstdio>

#include "art.hpp"

// Симметрия звезды, которой ОПЛАЧЕН отказ от личного поворота частицы (спека #17, вертикаль 3,
// шаг B3). Стоит отдельным файлом от голдена частиц намеренно: утверждение здесь про ПЕЧЁНЫЙ
// АТЛАС, а не про `Fx` с его пулами, и общего у них ровно ноль — свести их в один гейт значило бы
// получить файл, краснеющий на правке траектории там, где спрашивают про картинку.
namespace {

int32_t fails = 0;

void check(bool ok, const char* what) {
    if (!ok) { std::printf("  FAIL: %s\n", what); ++fails; }
}

const game::Atlas& game_atlas() {
    static const game::Atlas a = game::build_atlas();
    return a;
}

// Отказ от личного поворота частицы законен ровно постольку, поскольку повёрнутая звезда неотличима
// от неповёрнутой. Утверждение проверяемо: регион звезды квадратный, и его альфа обязана совпадать
// с собой под отражениями по обеим осям И под транспонированием. Первых двух мало — четырёхлучевая
// «искра» переживает оба отражения и разъезжается только на транспонировании, то есть на повороте
// в 45°, который эмиттер как раз и раздаёт.
void test_star_symmetry() {
    const game::Atlas& a = game_atlas();
    // Полтексела ВОЗВРАЩАЮТСЯ. `game::rgn` уводит UV внутрь на `0.5/w` — приём для СЭМПЛЕРА, и
    // читать по нему пиксели значит взять окно шириной на пиксель меньше, сдвинутое на полпикселя
    // относительно настоящего центра. Первая версия гейта так и сделала и отбила симметрию по
    // отражениям (18 из 255) при нулевом расхождении по транспонированию — то есть обвинила
    // картинку в том, что натворил её собственный обход.
    auto lo = [](float uv, uint32_t size) {
        return static_cast<uint32_t>(uv * static_cast<float>(size) - 0.5f + 0.5f);
    };
    auto hi = [](float uv, uint32_t size) {
        return static_cast<uint32_t>(uv * static_cast<float>(size) + 0.5f + 0.5f);
    };
    const uint32_t x0 = lo(a.star.u0, a.w), y0 = lo(a.star.v0, a.h);
    const uint32_t x1 = hi(a.star.u1, a.w), y1 = hi(a.star.v1, a.h);
    const uint32_t n = x1 - x0;
    const int32_t before = fails;
    check(n > 0 && n == y1 - y0, "the star region is a non-empty square");
    check(a.px.size() >= static_cast<size_t>(a.w) * a.h * 4, "the procedural page carries pixels");
    // Выход по СВОИМ находкам, а не по глобальному счётчику: тот к этому месту уже несёт чужие, и
    // проверка симметрии молча пропускалась бы каждый раз, когда красен голден выше.
    if (fails != before) return;

    auto alpha = [&](uint32_t x, uint32_t y) {
        return a.px[(static_cast<size_t>(y0 + y) * a.w + (x0 + x)) * 4 + 3];
    };
    int32_t worst_mx = 0, worst_my = 0, worst_tr = 0, span = 0;
    for (uint32_t y = 0; y < n; ++y) {
        for (uint32_t x = 0; x < n; ++x) {
            const int32_t v = alpha(x, y);
            if (v > span) span = v;
            const int32_t mx = v - alpha(n - 1 - x, y);
            const int32_t my = v - alpha(x, n - 1 - y);
            const int32_t tr = v - alpha(y, x);
            if (std::abs(mx) > worst_mx) worst_mx = std::abs(mx);
            if (std::abs(my) > worst_my) worst_my = std::abs(my);
            if (std::abs(tr) > worst_tr) worst_tr = std::abs(tr);
        }
    }
    std::printf("  star %ux%u: mirror-x %d, mirror-y %d, transpose %d (span %d)\n", n, n, worst_mx,
                worst_my, worst_tr, span);
    // Порог, а не ноль: альфа — восьмибитная развёртка `pow(1 - d/r, 1.8)`, и округление двух
    // расстояний, равных с точностью до float, законно расходится на единицу.
    check(worst_mx <= 1 && worst_my <= 1 && worst_tr <= 1, "the star is symmetric under rotation");
    // Позитивный контроль симметрии: сплошной прямоугольник тоже симметричен, и порог выше прошёл бы
    // на нём с тем же нулём. Звезда обязана быть ГРАДИЕНТОМ — иначе доказывать нечего.
    check(span > 200, "the star actually has opaque pixels");
    check(alpha(0, 0) == 0 && alpha(n / 2, n / 2) == span, "the star falls off from its centre");
}

} // namespace

int main() {
    test_star_symmetry();
    std::printf("game-star: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
