#include <cstdio>
#include <vector>

#include "platform_args.hpp"
#include "query.hpp"

// Односторонний тайл (вертикаль 3, шаг B): ЗАПРОС решает по хиту, держит ли платформа.
//
// Своей целью от геометрии склона и от окна `..._test` по тому же основанию: предмет здесь не форма
// и не маска, а ПРАВИЛО ОТБОРА хита, и имя упавшей цели в логе CI обязано это называть.
//
// Каждое утверждение парное, и пара не косметика: «свип не нашёл платформу» — правда и про пустую
// сетку, и про промах зонда, и про сломанный обход окна. Поэтому та же раскладка, тот же зонд и тот
// же путь прогоняются по ТЕЛЕСНОМУ тайлу, который обязан ответить всегда: расходятся они ровно
// одним битом карты.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::tilemap;

fix32 fx(int v) { return fix32::from_int(v); }

// Тайл (3,3) занимает x [48,64], y [48,64] — верх его на y = 48, и все числа ниже мерятся от него.
constexpr uint32_t TX = 3;
constexpr uint32_t TY = 3;

TileGrid one_tile(TileFlags flags) {
    TileGrid g({fix32{}, fix32{}}, fx(16), 8, 8);
    g.set(TX, TY, flags);
    return g;
}

physics::Shape mover() { return physics::sanitize(physics::box(fx(6), fx(6))); }

// Свип формы из `from` на `travel`. Возвращает, случилось ли касание.
bool sweep(const TileGrid& g, Vec2 from, Vec2 travel, TileFlags exclude = TILE_EMPTY) {
    TileFilter f;
    f.exclude = exclude;
    TileHit hit;
    return shapecast(g, mover(), from, fix32{}, travel, f, hit);
}

constexpr TileFlags ONEWAY = TILE_SOLID | TILE_ONEWAY;

// Пути названы от тайла, а не числами в месте вызова: «сверху вниз» и «снизу вверх» — это одно
// утверждение о ГРАНЯХ, и разъехаться двум его половинам негде, пока они берут одну пару констант.
struct Approach {
    const char* name;
    Vec2 from;
    Vec2 travel;
    bool oneway_holds;   // держит ли ОДНОСТОРОННИЙ; телесный держит всегда
};

const Approach APPROACHES[] = {
    // Сверху: низ формы на старте 42, то есть выше верха тайла (48). Единственный, кого платформа
    // обязана держать.
    {"from above", {fx(56), fx(36)}, {fix32{}, fx(20)}, true},
    // Снизу: форма стоит под тайлом, низ её 86 — уже ниже верха. Держащая грань смотрит ВНИЗ, и
    // правило снимает её первым же условием.
    {"from below", {fx(56), fx(80)}, {fix32{}, fx(-20)}, false},
    // Сбоку, на высоте тайла: грань встречная, но смотрит вбок. Персонаж, бегущий вдоль ряда
    // платформ, обязан проходить их насквозь, а не спотыкаться о торцы.
    {"from the side", {fx(30), fx(56)}, {fx(40), fix32{}}, false},
    // Сверху вниз, но ноги УЖЕ ниже верха (низ формы 52): так выглядит тик после спуска. Держать
    // здесь значило бы втащить провалившегося обратно на платформу.
    {"downward from inside", {fx(56), fx(46)}, {fix32{}, fx(20)}, false},
    // Наискось сверху-слева в БОКОВУЮ грань: ноги на старте выше верха тайла (42 против 48), то
    // есть условие про ноги здесь выполнено, и отбивает случай только направление грани. Без него
    // персонаж, бегущий по воздуху мимо торца платформы, упирался бы в него, как в стену. Случай
    // выписан отдельно потому, что три предыдущих отбиваются условием про ноги и по ним проверка
    // граней выглядит лишней: гейт без него молчал бы на реализации, потерявшей её целиком.
    {"diagonally into the side", {fx(30), fx(36)}, {fx(30), fx(24)}, false},
};

void test_faces_and_feet() {
    const TileGrid oneway = one_tile(ONEWAY);
    const TileGrid solid = one_tile(TILE_SOLID);
    for (const Approach& a : APPROACHES) {
        const bool got = sweep(oneway, a.from, a.travel);
        const bool control = sweep(solid, a.from, a.travel);
        std::printf("  %-21s oneway=%d solid=%d (expected oneway=%d)\n", a.name, got ? 1 : 0,
                    control ? 1 : 0, a.oneway_holds ? 1 : 0);
        // Пара: тот же зонд по телесному тайлу обязан ответить. Без неё «односторонний не держит»
        // проходило бы и у обхода, потерявшего окно целиком.
        check(control, "pair: the same sweep does hit a plain solid tile");
        check(got == a.oneway_holds, a.oneway_holds
                                         ? "a one-way tile holds the sweep that comes from above"
                                         : "and holds no other approach");
    }
}

// Спуск по команде. Контроллер снимает односторонние тайлы фильтром на ОДИН тик, и снимать их
// обязан именно фильтр: правило граней тут бессильно — ноги спускающегося на старте тика ещё выше
// верха платформы, то есть по правилу она держит.
void test_exclude_drops_through() {
    const TileGrid g = one_tile(ONEWAY);
    const Approach& above = APPROACHES[0];
    check(sweep(g, above.from, above.travel), "precondition: without the filter the tile holds");
    check(!sweep(g, above.from, above.travel, TILE_ONEWAY),
          "excluding TILE_ONEWAY drops the same sweep through");
    // Пара: исключение обязано снимать ТОЛЬКО односторонние. Иначе спускающийся проваливался бы и
    // сквозь камень под платформой.
    const TileGrid solid = one_tile(TILE_SOLID);
    check(sweep(solid, above.from, above.travel, TILE_ONEWAY),
          "pair: and leaves a plain solid tile holding");
}

// «Кто здесь СЕЙЧАС» — вопрос без пути, и правила граней у него нет: односторонний тайл телесен, и
// перекрытие обязано его называть. Утверждение не теоретическое — им пользуется всякий запрос,
// спрашивающий `solid`, и молчащее здесь перекрытие означало бы дырку в карте.
void test_overlap_still_sees_it() {
    const TileGrid g = one_tile(ONEWAY);
    std::vector<TileOverlap> out;
    TileFilter f;
    overlap_shape(g, mover(), {fx(56), fx(56)}, fix32{}, f, out);
    check(out.size() == 1, "overlap_shape reports a one-way tile like any other solid one");
    // Пара по тому же зонду: телесный тайл в той же точке отвечает так же, значит «один» выше
    // сказано про правило, а не про то, что зонд случайно попал в одну клетку.
    const TileGrid solid = one_tile(TILE_SOLID);
    overlap_shape(solid, mover(), {fx(56), fx(56)}, fix32{}, f, out);
    check(out.size() == 1, "pair: exactly as a plain solid tile does");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("tilemap one-way faces gate\n");
    test_faces_and_feet();
    test_exclude_drops_through();
    test_overlap_still_sees_it();
    std::printf("framework-tilemap-oneway: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
