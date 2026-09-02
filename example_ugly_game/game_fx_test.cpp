#include <cstdio>

#include "art.hpp"
#include "ecs_alloc_probe.hpp"
#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "fx.hpp"

// Частицы шутера НА ФРЕЙМВОРКЕ (спека #17, вертикаль 3, шаг B3). Два утверждения, и одно не
// заменяет другое: сцена повторяется числом (голден) и установившийся кадр не ходит в кучу
// (гейт 8). Симметрия звезды, которой оплачен отказ от личного поворота частицы, живёт своим
// файлом (`game_star_test`) — она про печёный атлас, а не про `Fx`.
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
void frame(game::FxSink& sink, const game::TrailQuery& trails) {
    fx.emit(sink);
    fx.emit_trails(trails);
    fx.update();
    fx.draw(game_atlas(), insts, game::FX_CAP);
}

constexpr uint32_t TICKS = 120;
constexpr uint64_t GOLDEN = 0xfd9ca7d2936ad48aull;

void test_golden() {
    std::printf("shooter particles: scripted run over %u ticks\n", TICKS);
    game::FxSink sink;
    flecs::world world = make_world();
    const game::TrailQuery trails = game::make_trail_query(world);
    fx.clear();
    uint32_t peak = 0;
    for (uint32_t t = 0; t < TICKS; ++t) {
        fill_events(sink, t);
        frame(sink, trails);
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
    check(fx.peak() == peak, "the emitter's own high-water mark is the one measured here");
}

// ГЕЙТ 8 на игровом пути: ВЕСЬ игровой кадр (`emit` + `emit_trails` + `update` + `draw`), а не
// фреймворковая его половина. Обход сущностей стоял снаружи «чтобы не обвинить ECS», и защищало это
// НИЧЕГО: счётчик фреймворка подменяет `operator new`, а flecs ходит в кучу через `ecs_os_api` и
// мимо него — ноль там значил «не смотрю». Счётчиков поэтому два, и оба обязаны быть нулём.
void test_no_alloc() {
    game::FxSink sink;
    flecs::world world = make_world();
    const game::TrailQuery trails = game::make_trail_query(world);
    fx.clear();
    // Первые кадры — ДО счётчика: ленивая инициализация мира, векторов приёмника, статического
    // атласа и таблиц, которые запрос сопоставляет на первом обходе, оплачивается заранее.
    for (uint32_t t = 0; t < 8; ++t) { fill_events(sink, t); frame(sink, trails); }
    fill_events(sink, 0);
    const uint32_t before = fx.count();

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    game::ecs_probe::allocs = 0;
    for (uint32_t t = 0; t < 16; ++t) frame(sink, trails);
    const long during = framework::probe::allocs;
    const long during_ecs = game::ecs_probe::allocs;
    framework::probe::in_hot = false;

    std::printf("  steady frames: allocations = %ld (ecs %ld), alive %u -> %u\n", during,
                during_ecs, before, fx.count());
    check(during == 0, "the steady game frame does not touch the heap");
    check(during_ecs == 0, "the steady game frame does not touch the ECS heap");
    check(fx.count() > before, "the measured frames actually emitted particles");
    check(fx.dropped() == 0, "the measured frames drew a full quota");

    framework::probe::in_hot = true;
    framework::probe::allocs = 0;
    const bool plain_ok = framework::probe::control::plain_allocation();
    const long plain = framework::probe::allocs;
    framework::probe::allocs = 0;
    const bool aligned_ok = framework::probe::control::aligned_allocation();
    const long aligned = framework::probe::allocs;
    game::ecs_probe::allocs = 0;
    ecs_os_free(ecs_os_malloc(64));
    const long ecs_ctl = game::ecs_probe::allocs;
    framework::probe::in_hot = false;
    check(plain_ok && plain > 0, "control: the counter sees a plain allocation");
    check(aligned_ok && aligned > 0, "control: the counter sees an over-aligned allocation");
    // Тот же контроль второму счётчику: незаряженный шов отвечает нулём честного кадра.
    check(ecs_ctl > 0, "control: the counter sees an ECS allocation");
}

} // namespace

int main() {
    game::ecs_probe::install();   // ДО первого мира: таблицу он забирает при создании
    test_golden();
    test_no_alloc();
    std::printf("game-fx: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
