#include <cmath>
#include <cstdio>

#include "art.hpp"
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "fx.hpp"

// Частицы шутера НА ФРЕЙМВОРКЕ (спека #17, вертикаль 3, шаг B3). Три утверждения, и ни одно не
// заменяет другие: сцена повторяется числом (голден), установившийся кадр не ходит в кучу (гейт 8)
// и звезда, которой рисуется частица, радиально симметрична — то есть отказ от ЛИЧНОГО ПОВОРОТА
// частицы, который был у старого float-`fx.cpp`, невидим по построению картинки, а не «на глаз».
//
// Кадр здесь не проверяется: он про экспозицию и UV и стоит отдельным швом
// (`game_sprite_out_test`). Слить их значило бы получить хеш, краснеющий на правке материала ровно
// так же, как на правке траектории, — то есть не говорящий ни о чём.
namespace {

int32_t fails = 0;

void check(bool ok, const char* what) {
    if (!ok) { std::printf("  FAIL: %s\n", what); ++fails; }
}

// Пулы у `Fx` собственные и живут внутри объекта (гейт 8), поэтому он статический: сорок килобайт
// на кадровом стеке — не то, на чём стоит проверять отсутствие аллокаций.
game::Fx fx;
game::Instance insts[game::FX_CAP];

const game::Atlas& game_atlas() {
    static const game::Atlas a = game::build_atlas();
    return a;
}

void mix(uint64_t& h, int64_t v) {
    for (int32_t i = 0; i < 8; ++i) {
        h ^= static_cast<uint64_t>((v >> (i * 8)) & 0xff);
        h *= 0x100000001b3ull;
    }
}

// Поля перечислены ПОИМЁННО, а не байтами структуры: `Particle` дополнялся в этом же раунде
// (`life`, `scale`), и голден по байтам краснел бы от вставки поля, ничего не сказав про поведение.
uint64_t fold(const game::Fx& e) {
    uint64_t h = 0xcbf29ce484222325ull;
    mix(h, e.count());
    mix(h, e.dropped());
    mix(h, e.stream());
    for (uint32_t i = 0; i < e.count(); ++i) {
        const framework::graphics::Particle& p = e.at(i);
        mix(h, p.pos.x.raw);
        mix(h, p.pos.y.raw);
        mix(h, p.vel.x.raw);
        mix(h, p.vel.y.raw);
        mix(h, p.age.raw);
        mix(h, p.life.raw);
        mix(h, p.scale.raw);
        mix(h, p.desc);
    }
    return h;
}

// Мир из трёх сущностей вместо живого `spawn()`: тот ведёт целую игру, и голден частиц зависел бы
// от правки баланса врагов — то есть краснел бы там, где частицы не менялись.
flecs::world make_world() {
    flecs::world w;
    w.entity()
        .set<game::Transform>({fix32::from_int(120), fix32::from_int(300)})
        .set<game::Velocity>({fix32{}, fix32{}})
        .add<game::Ship>();
    for (int32_t i = 0; i < 2; ++i) {
        w.entity()
            .set<game::Transform>({fix32::from_int(400 + i * 90), fix32::from_int(220 + i * 60)})
            .set<game::Velocity>({fix32::from_int(9), fix32{}})
            .add<game::Bullet>();
    }
    return w;
}

// Расписание боя: залпы каждые 12 тиков плюс по одному событию каждого рода. Все пять родов названы
// намеренно — таблица описаний `fx_table()` заполняется восемью строками, и голден, не тронувший
// половину, пустил бы опечатку в них молча.
void fill_events(game::FxSink& sink, uint32_t t) {
    sink.events.clear();
    if (t % 12 == 0) sink.events.push_back({130.0f, 300.0f, game::FX_Fire});
    if (t == 10) sink.events.push_back({520.0f, 240.0f, game::FX_EnemyDie});
    if (t == 30) sink.events.push_back({700.0f, 260.0f, game::FX_BossHit});
    if (t == 60) sink.events.push_back({700.0f, 260.0f, game::FX_BossDie});
    if (t == 90) sink.events.push_back({120.0f, 300.0f, game::FX_PlayerHit});
}

// Кадр ЦЕЛИКОМ и в том же порядке, в каком его гоняют `live.cpp` и `demo.cpp`: события боя, следы,
// шаг, выкладка. Кадр, собранный в гейте иначе, проверял бы не ту игру.
void frame(game::FxSink& sink, flecs::world& world) {
    fx.emit(sink);
    fx.emit_trails(world);
    fx.update();
    fx.draw(game_atlas(), insts, game::FX_CAP);
}

constexpr uint32_t TICKS = 120;
constexpr uint64_t GOLDEN = 0xfd9ca7d2936ad48aull;

void test_golden() {
    std::printf("shooter particles: scripted run over %u ticks\n", TICKS);
    game::FxSink sink;
    flecs::world world = make_world();
    fx.clear();
    uint32_t peak = 0;
    for (uint32_t t = 0; t < TICKS; ++t) {
        fill_events(sink, t);
        frame(sink, world);
        if (fx.count() > peak) peak = fx.count();
    }
    const uint64_t h = fold(fx);
    std::printf("  alive: %u, peak: %u, dropped: %u\n", fx.count(), peak, fx.dropped());
    std::printf("  shooter particle hash = 0x%016llx\n", static_cast<unsigned long long>(h));
    check(h == GOLDEN, "hash matches the golden");
    // Позитивные контроли голдена: прогон, из которого частицы разошлись, дал бы стабильный ноль, а
    // прогон, переполнивший пул, — стабильную полную ёмкость. И то и другое — «повторяется».
    // Пик берётся по ходу, а не в конце: залп босса из шестидесяти живёт полсекунды и к последнему
    // тику успевает истечь, поэтому итоговый остаток о взрывах не говорит вообще ничего.
    check(fx.count() > 30, "the run leaves a populated pool");
    check(peak > 100, "the bursts of the run actually landed");
    check(peak < game::FX_CAP, "the pool is not saturated");
    check(fx.dropped() == 0, "no burst was truncated");
}

// ГЕЙТ 8 на игровом пути. Мерится ФРЕЙМВОРКОВАЯ половина кадра: `emit` + `update` + `draw`. Обход
// сущностей (`emit_trails`) стоит снаружи намеренно — запрос ведёт flecs, и его аллокации сказали
// бы про ECS, а не про то, ради чего пулы у `Fx` стали массивами.
void test_no_alloc() {
    game::FxSink sink;
    flecs::world world = make_world();
    fx.clear();
    // Первые кадры — ДО счётчика: ленивая инициализация мира, векторов приёмника и статического
    // атласа обязана быть оплачена заранее, иначе гейт обвинил бы в аллокации установившийся кадр.
    for (uint32_t t = 0; t < 8; ++t) { fill_events(sink, t); frame(sink, world); }
    fill_events(sink, 0);
    fx.emit_trails(world);
    const uint32_t before = fx.count();

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    for (uint32_t t = 0; t < 16; ++t) {
        fx.emit(sink);
        fx.update();
        fx.draw(game_atlas(), insts, game::FX_CAP);
    }
    const long during = framework::probe::allocs;
    framework::probe::in_hot = false;

    std::printf("  steady frames: allocations = %ld, alive %u -> %u\n", during, before, fx.count());
    check(during == 0, "the steady framework frame does not touch the heap");
    check(fx.count() > before, "the measured frames actually emitted particles");
    check(fx.dropped() == 0, "the measured frames drew a full quota");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool plain_ok = framework::probe::control::plain_allocation();
    const long plain = framework::probe::allocs;
    framework::probe::allocs = 0;
    const bool aligned_ok = framework::probe::control::aligned_allocation();
    const long aligned = framework::probe::allocs;
    framework::probe::in_hot = false;
    check(plain_ok && plain > 0, "control: the counter sees a plain allocation");
    check(aligned_ok && aligned > 0, "control: the counter sees an over-aligned allocation");
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
    test_golden();
    test_no_alloc();
    test_star_symmetry();
    std::printf("game-fx: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
