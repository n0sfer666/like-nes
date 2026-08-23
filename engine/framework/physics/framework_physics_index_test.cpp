#include <cstdio>

#include "platform_args.hpp"
#include "query.hpp"

// Индекс запросов: полоса вместо обхода мира (спека #16, перф-раунд перед вертикалью 2). Отдельной
// целью от трёх гейтов запросов, потому что вопрос у неё третий по счёту и ни одним из них не
// задаётся: те спрашивают «верен ли ответ», голдены — «сошлись ли три ОС», а этот — «во сколько
// ответ обошёлся и не устарел ли он».
//
// Оба утверждения тут двусторонние, и односторонние были бы вакуумны:
//
//   цена     — узкий зонд обязан рассмотреть единицы тел, но ШИРОКИЙ обязан рассмотреть все.
//              Счётчик, прибитый к маленькому числу, первую половину проходит и валится на второй;
//   свежесть — правка через `mutate` обязана быть видна СРАЗУ, и запрос до правки обязателен:
//              без него индекс строится уже после неё, и гейт зелен при выброшенной инвалидации.
//
// Дверей протухания ТРИ, и каждая проверяется своей сценой: правка (`mutate`), добавление (`add`) и
// ШАГ. Третья не выводится из первых двух и без собственной сцены не проверялась ничем: ни один
// гейт дерева не звал запрос ПОСЛЕ шага, поэтому строку инвалидации в `World::step` можно было
// снять, оставив всю связку зелёной, — а игра получила бы индекс, замороженный на первом кадре.
// Дверь, срабатывающая шестьдесят раз в секунду, оказалась единственной без позитивного контроля.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::physics;

// Колоннада вдоль X: сцена, на которой полоса вообще имеет смысл. Тела разнесены так, что зонд
// размером с одно накрывает ровно одно, — на куче, где все AABB пересекаются, индекс обязан честно
// вернуть всех, и мерить на ней было бы нечего.
constexpr uint32_t COLUMNS = 192;
constexpr int32_t SPACING = 64;
constexpr int32_t LEFT = -6144;
constexpr int32_t HALF = 8;

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);

fix32 column_x(uint32_t i) { return fix32::from_int(LEFT + static_cast<int32_t>(i) * SPACING); }

void build(World& w, uint32_t columns) {
    for (uint32_t i = 0; i < columns; ++i) {
        BodyDesc d;
        d.key = i + 1;
        d.type = BodyType::Static;
        d.shape = box(fix32::from_int(HALF), fix32::from_int(HALF));
        d.position = {column_x(i), fix32{}};
        w.add(d);
    }
}

uint32_t only_key(const std::vector<Overlap>& out) { return out.size() == 1 ? out[0].key : 0; }

// Цена. Число рассмотренных — это то, что запрос перебрал СВЕРХ двоичного поиска, и линейный обход
// дал бы здесь `COLUMNS` на любом зонде. Потолок в четыре тела, а не в одно: он переживёт правку
// спекулятивного поля, но не переживёт возврата к обходу мира — а различать гейт обязан именно это.
void test_a_narrow_probe_scans_a_handful() {
    World w(COLUMNS);
    build(w, COLUMNS);
    QueryFilter f;
    std::vector<Overlap> out;

    for (uint32_t i = 0; i < COLUMNS; ++i) {
        overlap_shape(w, box(fix32::from_int(4), fix32::from_int(4)), {column_x(i), fix32{}},
                      fix32{}, f, out);
        if (only_key(out) != i + 1) {
            check(false, "a probe over a column answers exactly that column");
            std::printf("  column %u answered %zu bodies\n", i, out.size());
            break;
        }
        if (w.query_counters().scanned > 4) {
            check(false, "and pays a handful of bodies for it, not the whole world");
            std::printf("  column %u scanned %llu of %u\n", i,
                        static_cast<unsigned long long>(w.query_counters().scanned), COLUMNS);
            break;
        }
    }
    std::printf("  narrow probe scanned %llu of %u bodies\n",
                static_cast<unsigned long long>(w.query_counters().scanned), COLUMNS);

    // Зонд, накрывающий шов между колоннами, обязан вернуть ОБЕ. Крайние индексы взяты нарочно: у
    // полосы два конца, и оба они — граница двоичного поиска, а не середина, где ошибка на единицу
    // прячется за соседом.
    const int32_t seam = SPACING / 2;
    overlap_shape(w, box(fix32::from_int(seam), fix32::from_int(HALF)),
                  {column_x(0) + fix32::from_int(seam), fix32{}}, fix32{}, f, out);
    check(out.size() == 2 && out[0].key == 1 && out[1].key == 2,
          "a probe over the first seam answers both columns it touches");
    overlap_shape(w, box(fix32::from_int(seam), fix32::from_int(HALF)),
                  {column_x(COLUMNS - 2) + fix32::from_int(seam), fix32{}}, fix32{}, f, out);
    check(out.size() == 2 && out[0].key == COLUMNS - 1 && out[1].key == COLUMNS,
          "and over the last seam the same, at the other end of the band");
}

// Вторая половина той же меры. Луч поперёк всей сцены обязан рассмотреть её ЦЕЛИКОМ: полоса шире
// мира — это не «индекс сломался», это честный ответ на широкий вопрос. Без этого утверждения
// счётчик, возвращающий единицу всегда, проходил бы гейт цены выше.
void test_a_wide_cast_scans_the_world() {
    World w(COLUMNS);
    build(w, COLUMNS);
    QueryFilter f;
    RayHit hit;
    const fix32 y = fix32{};
    const bool found = raycast(w, {fix32::from_int(LEFT - SPACING), y},
                               {fix32::from_int(static_cast<int32_t>(COLUMNS) * SPACING), y}, f,
                               hit);
    check(found && hit.key == 1, "a ray across the colonnade stops at the nearest column");
    check(w.query_counters().scanned == COLUMNS, "and its band covers every body in the scene");
    std::printf("  wide ray scanned %llu of %u bodies\n",
                static_cast<unsigned long long>(w.query_counters().scanned), COLUMNS);
}

// Свежесть. Запрос ДО правки здесь обязателен и стоит первым: он строит индекс, и только после него
// вопрос «заметил ли индекс правку» вообще задан. Шага между правкой и вторым запросом нет
// намеренно — шаг метит индекс протухшим сам и подменил бы собой проверяемый шов.
void test_the_index_does_not_go_stale() {
    World w(COLUMNS);
    build(w, 4);
    QueryFilter f;
    std::vector<Overlap> out;
    const Shape probe = box(fix32::from_int(4), fix32::from_int(4));
    const fix32 was = column_x(1);
    const fix32 now = fix32::from_int(4096);

    overlap_shape(w, probe, {was, fix32{}}, fix32{}, f, out);
    check(only_key(out) == 2, "the column is where the scene put it before anything is moved");

    w.mutate(BodyId{1}).position.x = now;
    overlap_shape(w, probe, {was, fix32{}}, fix32{}, f, out);
    check(out.empty(), "after mutate the query no longer finds it at the old place");
    overlap_shape(w, probe, {now, fix32{}}, fix32{}, f, out);
    check(only_key(out) == 2, "and does find it at the new one, without a step in between");

    // Тот же шов вторым путём запроса: свип и перекрытие ходят в индекс порознь, и забытая
    // инвалидация одного из них не видна другому.
    RayHit hit;
    const bool found = raycast(w, {now, fix32::from_int(-64)}, {fix32{}, fix32::from_int(128)}, f,
                               hit);
    check(found && hit.key == 2, "a ray fired at the new place hits it too");
}

// Третья дверь: ШАГ. Тело здесь динамическое и летит по X — единственная сцена файла, где мир
// двигает тело сам, без единой правки снаружи. Запрос до шага обязателен ровно по той же причине,
// что и в сцене выше: он строит индекс, и только после него вопрос вообще задан.
void test_a_step_moves_the_body_out_of_the_index() {
    World w(4);
    w.set_gravity({fix32{}, fix32{}});
    BodyDesc d;
    d.key = 5;
    d.type = BodyType::Dynamic;
    d.shape = box(fix32::from_int(HALF), fix32::from_int(HALF));
    d.position = {fix32{}, fix32{}};
    const BodyId id = w.add(d);
    w.mutate(id).velocity = {fix32::from_int(600), fix32{}};

    QueryFilter f;
    std::vector<Overlap> out;
    const Shape probe = box(fix32::from_int(4), fix32::from_int(4));
    overlap_shape(w, probe, {fix32{}, fix32{}}, fix32{}, f, out);
    check(only_key(out) == 5, "the moving body starts where the scene put it");

    for (int i = 0; i < 30; ++i) w.step(DT);
    const fix32 now = w.body(id).position.x;
    check(fix32::from_int(4 * HALF) < now, "and a second of stepping takes it clear of its start");

    overlap_shape(w, probe, {fix32{}, fix32{}}, fix32{}, f, out);
    check(out.empty(), "after the step the query no longer finds it at the old place");
    overlap_shape(w, probe, {now, fix32{}}, fix32{}, f, out);
    check(only_key(out) == 5, "and does find it where the step put it");
}

// Добавление тела — тоже правка раскладки, и забыть её страшнее: не отставший, а НЕВИДИМЫЙ ответ.
void test_a_new_body_enters_the_index() {
    World w(COLUMNS);
    build(w, 2);
    QueryFilter f;
    std::vector<Overlap> out;
    const Shape probe = box(fix32::from_int(4), fix32::from_int(4));
    const fix32 spot = fix32::from_int(2048);

    overlap_shape(w, probe, {spot, fix32{}}, fix32{}, f, out);
    check(out.empty(), "the spot the new body will take is empty first");

    BodyDesc d;
    d.key = 777;
    d.type = BodyType::Static;
    d.shape = box(fix32::from_int(HALF), fix32::from_int(HALF));
    d.position = {spot, fix32{}};
    w.add(d);

    overlap_shape(w, probe, {spot, fix32{}}, fix32{}, f, out);
    check(only_key(out) == 777, "and a body added there is visible to the very next query");
}

// Граница ДОПУСКА свипа — место, где отсечение по полосе разошлось с линейным обходом на первом же
// прогоне этого раунда. Форма, стоящая от тела ровно на `CONTACT_SLOP`, для свипа уже касается: он
// останавливается на этом расстоянии. Для `overlaps` она уже НЕ пересекается: сравнение строгое.
// Значит расширять надо обе коробки, а не одну, — и вопрос этот принадлежит физике, хотя измерен он
// был контролем гейта туннелирования в `character`, который вместо «зажат» показал четыре юнита.
void test_the_band_keeps_the_sweep_tolerance() {
    World w(4);
    build(w, 1);
    QueryFilter f;
    RayHit hit;
    const fix32 touching = column_x(0) + fix32::from_int(2 * HALF) + CONTACT_SLOP;
    const bool found = shapecast(w, box(fix32::from_int(HALF), fix32::from_int(HALF)),
                                 {touching, fix32{}}, fix32{}, {fix32{}, fix32::from_int(-4)}, f,
                                 hit);
    check(found && hit.key == 1, "a sweep exactly one slop away from a body still sees it");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics query index gate\n");
    test_a_narrow_probe_scans_a_handful();
    test_a_wide_cast_scans_the_world();
    test_the_band_keeps_the_sweep_tolerance();
    test_the_index_does_not_go_stale();
    test_a_step_moves_the_body_out_of_the_index();
    test_a_new_body_enters_the_index();
    std::printf("framework-physics-index: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
