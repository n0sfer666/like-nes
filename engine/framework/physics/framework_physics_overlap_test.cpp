#include <cstdio>

#include "platform_args.hpp"
#include "query.hpp"

// Гейт 4 спеки #15, третий вид запроса: перекрытие области. Отдельной целью от свипов
// (`framework_physics_query_test`) по той же причине, по которой узкая фаза разложена на три:
// у перекрытия свой вопрос — не «когда коснёмся», а «кто здесь сейчас», — и имя упавшей цели в
// логе CI обязано называть сломанный вид запроса, а не «запросы вообще».
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

BodyDesc wall(uint32_t key, fix32 x) {
    BodyDesc d;
    d.key = key;
    d.type = BodyType::Static;
    d.shape = box(fix32::from_int(8), fix32::from_int(8));
    d.position = {x, fix32{}};
    return d;
}

void test_overlap() {
    World w(8);
    // Ключи нарочно НЕ по порядку создания: ответ обязан прийти по возрастанию ключа, иначе гейт 2
    // (перетасованное создание) поймал бы запрос на расхождении с самим собой.
    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t keys[3] = {30, 10, 20};
        BodyDesc d;
        d.key = keys[i];
        d.shape = box(fix32::from_int(2), fix32::from_int(2));
        d.position = {fix32::from_int(static_cast<int32_t>(i) * 4 - 4), fix32{}};
        w.add(d);
    }
    w.add(wall(40, fix32::from_int(200)));

    QueryFilter f;
    std::vector<Overlap> out;

    overlap_shape(w, box(fix32::from_int(10), fix32::from_int(10)), {fix32{}, fix32{}}, fix32{}, f,
                  out);
    check(out.size() == 3, "the probe covers the three small boxes and not the far wall");
    check(out.size() == 3 && out[0].key == 10 && out[1].key == 20 && out[2].key == 30,
          "and answers by ascending key, not by creation order");

    // Тот же зонд, отведённый в пустоту: пусто, а не «последний ответ». `out` обязан очищаться.
    overlap_shape(w, box(fix32::from_int(10), fix32::from_int(10)),
                  {fix32::from_int(1000), fix32{}}, fix32{}, f, out);
    check(out.empty(), "a probe over empty space clears the previous answer");

    // Поворот зонда — тоже часть запроса. Тонкая коробка 12x1 вдоль оси накрывает все три; она же,
    // повёрнутая на четверть оборота, — только среднюю.
    const Shape bar = box(fix32::from_int(12), fix32::from_float(0.5));
    overlap_shape(w, bar, {fix32{}, fix32{}}, fix32{}, f, out);
    check(out.size() == 3, "an axis-aligned bar covers all three");
    overlap_shape(w, bar, {fix32{}, fix32{}}, fix32::from_float(0.25), f, out);
    check(out.size() == 1 && out[0].key == 10, "turned a quarter it covers only the middle one");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics overlap gate\n");
    test_overlap();
    std::printf("framework-physics-overlap: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
