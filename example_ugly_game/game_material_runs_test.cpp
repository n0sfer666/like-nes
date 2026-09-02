#include <cstdint>
#include <cstdio>
#include <vector>

#include "material_runs.hpp"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

uint32_t runs_of(const std::vector<uint16_t>& ids, std::vector<game::MaterialRun>& out) {
    out.assign(ids.size() + 1, game::MaterialRun{});
    return game::material_runs(ids.data(), static_cast<uint32_t>(ids.size()), out.data(),
                               static_cast<uint32_t>(out.size()));
}

// Гейт 5 спеки #18 на стороне игры: значения параметров едут ДАННЫМИ инстанса, поэтому десять
// врагов с разной силой вспышки делят материал и остаются ОДНИМ вызовом отрисовки. Утверждение
// стоит на числе вызовов, а не на картинке: из кадра «один вызов или десять» не видно вовсе.
void test_one_material_one_draw() {
    std::vector<game::MaterialRun> r;
    check(runs_of(std::vector<uint16_t>(10, 3), r) == 1, "ten sprites of one material draw once");
    check(r[0].first == 0 && r[0].count == 10 && r[0].material == 3, "the run covers all ten");
}

void test_empty_and_plain() {
    std::vector<game::MaterialRun> r;
    check(runs_of({}, r) == 0, "an empty frame draws nothing");
    check(runs_of(std::vector<uint16_t>(64, game::NO_MATERIAL), r) == 1,
          "a frame with no materials is still one draw");
}

// Порядок несущий: 2D-перекрытие ЕСТЬ порядок отрисовки, и группировка, собравшая одинаковые
// материалы со всего кадра, нарисовала бы другую картинку. Поэтому чередование обязано стоить
// по вызову на каждую смену — «оптимизация» до двух вызовов здесь есть дефект.
void test_order_is_kept() {
    std::vector<game::MaterialRun> r;
    const std::vector<uint16_t> ids = {1, 2, 1, 2, 1, 2};
    check(runs_of(ids, r) == 6, "alternating materials cost a draw each");
    for (uint32_t i = 0; i < 6; ++i)
        check(r[i].first == i && r[i].count == 1 && r[i].material == ids[i], "runs keep order");
}

// Каждый инстанс обязан быть нарисован РОВНО ОДИН РАЗ: прогоны, оставившие щель, теряют спрайт
// молча, а перекрывшиеся рисуют его дважды — и то и другое видно только этим утверждением.
void test_partition() {
    std::vector<game::MaterialRun> r;
    const std::vector<uint16_t> ids = {7, 7, 7, 0, 0, game::NO_MATERIAL, 4, 4};
    const uint32_t n = runs_of(ids, r);
    check(n == 4, "four stretches of equal material");
    uint32_t at = 0;
    for (uint32_t i = 0; i < n; ++i) {
        check(r[i].first == at, "a run starts where the previous ended");
        check(r[i].count > 0, "an empty run is not a run");
        at += r[i].count;
    }
    check(at == ids.size(), "the runs cover the frame");
}

// Потолок соблюдается: кадр, у которого прогонов больше, чем места, обязан оборваться, а не писать
// мимо буфера. Батч даёт буфер на MAX_INSTANCES, то есть худший случай покрыт, — но функция
// отвечает за это сама, иначе следующий её потребитель узнает про потолок стеком.
void test_cap() {
    const std::vector<uint16_t> ids = {1, 2, 3, 4, 5};
    game::MaterialRun out[2];
    check(game::material_runs(ids.data(), 5, out, 2) == 2, "the cap stops the walk");
    check(out[1].first == 1 && out[1].material == 2, "what fits is still correct");
}

} // namespace

int main() {
    test_one_material_one_draw();
    test_empty_and_plain();
    test_order_is_kept();
    test_partition();
    test_cap();
    // Числа гейта 5 печатаются, а не только проверяются: CI ассертит их литералом, и «сколько
    // вызовов стоит кадр» становится утверждением о движке, а не строкой PASS, за которой может
    // не стоять ни одной проверки.
    std::vector<game::MaterialRun> r;
    const uint32_t alike = runs_of(std::vector<uint16_t>(10, 3), r);
    const uint32_t alternating = runs_of({1, 2, 1, 2, 1, 2}, r);
    std::printf("draws: 10 alike = %u, 6 alternating = %u\n", alike, alternating);
    std::printf("game-material-runs: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
