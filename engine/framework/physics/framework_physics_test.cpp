#include <cstdio>
#include <vector>

#include "framework_physics_scene.hpp"
#include "platform_args.hpp"

// Гейт 1 спеки #15: состояние физики после фиксированного числа шагов даёт один и тот же хеш на
// macOS, Linux и Windows — эталон ниже проверяется в CI на каждой из трёх ОС.
//
// Один хеш гейтом быть не может: он ловит расхождение между платформами, но про сцену, где всё
// провалилось сквозь пол одинаково на всех трёх, скажет «зелено». Поэтому рядом с ним —
// наблюдаемые утверждения о поведении, каждое из которых падает на своей поломке: тела над полом
// (иначе туннелирование), стопка успокоилась (иначе накачка энергии решателем), трение съело
// горизонтальный ход (иначе касательный импульс не работает), тела повернулись (иначе момент
// считается и молча выбрасывается). Геометрия узкой фазы проверяется отдельной целью
// (`framework_physics_shape_test`) — она про верность манифольда, а не про сходимость сцены.
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

const Body* find(const World& w, uint32_t key) {
    for (const Body& b : w.bodies()) {
        if (b.key == key) return &b;
    }
    return nullptr;
}


void test_rest() {
    World w(fixture::CAPACITY);
    std::vector<BodyDesc> descs;
    fixture::describe(descs);
    fixture::fill(w, descs);
    fixture::run(w, fixture::STEPS);

    const fix32 floor_top = fix32::from_int(192);
    const fix32 quiet = fix32::from_int(2);
    fix32 lowest_bottom = -WORLD_HALF;
    bool anyone_turned = false;
    for (const Body& b : w.bodies()) {
        if (b.type != BodyType::Dynamic) continue;
        // Низ тела берётся из AABB, а не из полувысоты формы: у повёрнутого тела полувысоты нет
        // вовсе, а у капсулы и многоугольника её не было никогда. С +Y вниз низ — это `max.y`.
        const fix32 bottom = bounds(b).max.y;
        // Низ тела не ушёл заметно ниже верха пола: провал глубже допуска — это туннелирование
        // или решатель, не удержавший контакт, и то и другое обязано быть красным.
        check(bottom < floor_top + fix32::from_int(2), "body rests on the floor");
        check(abs_fix(b.velocity.y) < quiet, "vertical motion settled");
        check(abs_fix(b.position.x) < fix32::from_int(248), "body stayed between the walls");
        // Упало, а не зависло: стартовые высоты сцены отрицательные, пол — на +200. Без этой
        // строки «покоится» одинаково подписывалось бы под телом, застывшим в воздухе, — а
        // застывшее тело и есть самый частый вид сломанной интеграции.
        check(fix32::from_int(0) < b.position.y, "body actually fell toward the floor");
        // Угол приведён к периоду. Проверяется на КАЖДОМ теле, потому что уехавший за период угол
        // — это не косметика: он входит в хеш, и тело, провернувшееся на оборот больше, хешируется
        // иначе при том же положении на экране.
        check(!(b.angle < fix32{}) && b.angle < fix32::from_int(1), "angle stays inside one turn");
        check(abs_fix(b.angular_velocity) < quiet, "spin settled");
        if (b.angle.raw != 0) anyone_turned = true;
        lowest_bottom = max_fix(lowest_bottom, bottom);
    }
    // Хоть кто-то повернулся. Без этой строки вся вертикаль 2 проходила бы при решателе, который
    // считает момент и молча выбрасывает его: тела легли бы на пол ровно так же.
    check(anyone_turned, "rotation actually happened");
    // И хотя бы одно из них лежит НА полу, а не в стопке над ним: иначе вся куча могла зависнуть
    // на первом же контакте и всё равно пройти проверки выше.
    check(floor_top - fix32::from_int(2) < lowest_bottom, "the stack reaches the floor");

    // Трение: ящики стартовали с ходом 40 юнит/с вбок, лёгшая на шершавый пол стопка обязана
    // остановиться. Без касательного импульса они скользили бы вечно — гравитация им не мешает.
    for (uint32_t key = 10; key < 15; ++key) {
        const Body* b = find(w, key);
        check(b != nullptr && abs_fix(b->velocity.x) < quiet, "friction stopped the box");
    }
}

void test_static_immobile() {
    World w(fixture::CAPACITY);
    std::vector<BodyDesc> descs;
    fixture::describe(descs);
    fixture::fill(w, descs);
    const Vec2 floor_before = w.bodies()[0].position;
    fixture::run(w, fixture::STEPS);
    check(w.bodies()[0].position == floor_before, "static floor never moves");
}

// Эталон гейта 1. Снят с прогона, в котором ПРОШЛИ все проверки поведения выше, — и это
// единственное, что отличает эталон от закреплённого дефекта: хеш сцены, где всё провалилось
// сквозь пол, выглядит ровно так же убедительно. Меняется только вместе с осознанной сменой
// физики — числа итераций, порядка решения, формул. Разошёлся сам по себе — это находка, а не
// повод перезаписать константу.
//
// Перепечатан ОСОЗНАННО в вертикали 3 (`0xad1ec96e2dbbcc32` → значение ниже): шаг изменился в
// ЧЕТЫРЁХ местах сразу, и каждое меняет числа по построению.
//
//   1. Спекулятивный контакт: пара рождается уже при зазоре до 1/16, а не только на перекрытии.
//   2. Симметричный проход Гаусса—Зейделя: порядок точек внутри манифольда чередуется по итерациям,
//      что и убирает систематический занос.
//   3. Правило покоя: замерший остров получает РОВНО нулевую скорость вместо остаточных ±2 raw.
//   4. Округление `fix32::operator*` к НУЛЮ вместо арифметического сдвига вправо (`fixed.hpp`).
//
// Четвёртая причина стоит особняком, и назвать её здесь важнее прочих трёх: она единственная лежит
// ВНЕ физики. Сдвиг вправо округляет к минус бесконечности, поэтому произведение меньше кванта
// давало −1 raw на отрицательном знаке и 0 на положительном той же величины — асимметрия знака в
// ядре движка. Физике она стоила гейтов 3 и 5 (стопка ползла вверх, окно покоя не закрывалось
// никогда), но перепечатала ДВА чужих голдена — ядра и ввода, — и именно поэтому её нельзя было
// принять внутри этой цели: разбор и решение записаны в ADR раунда #15 и в спеке #11, «Гейты
// приёмки». Все проверки поведения в этой цели прошли на том же прогоне, с которого снят эталон.
//
// Перепечатан ОСОЗНАННО второй раз (`0xbeb8e8bee1bba145` → значение ниже) вместе с подъёмом
// `VELOCITY_ITERATIONS` с 8 до 16 — то есть ровно по названной выше причине «осознанная смена числа
// итераций». Замер, вынудивший подъём, лежит в `solver.hpp`: башня заваливалась с ДЕВЯТИ ящиков при
// штатном трении, и цифру глубины теперь держит собственный гейт (`framework_physics_depth_test`).
// Эталон снят с прогона, где прошли все проверки поведения в этой цели и во всех остальных целях
// физики, включая перепроверенные гейты стопки и пробуждения.
constexpr uint64_t GOLDEN = 0xe243e1457a11f65eULL;

void test_golden() {
    World w(fixture::CAPACITY);
    std::vector<BodyDesc> descs;
    fixture::describe(descs);
    fixture::fill(w, descs);
    fixture::run(w, fixture::STEPS);
    const uint64_t h = w.hash();
    std::printf("  physics-state-hash = 0x%016llx\n", static_cast<unsigned long long>(h));
    check(h == GOLDEN, "physics state hash matches the golden");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics gate\n");
    test_rest();
    test_static_immobile();
    test_golden();
    std::printf("framework-physics: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
