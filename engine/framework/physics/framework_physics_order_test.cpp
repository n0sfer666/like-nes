#include <algorithm>
#include <cstdio>
#include <vector>

#include "framework_physics_scene.hpp"
#include "platform_args.hpp"

// Гейт 2 спеки #15: перетасованный порядок СОЗДАНИЯ тел даёт тот же хеш состояния.
//
// Это не проверка сортировки — это проверка того, что порядок решения контактов задан стабильными
// ключами, а не индексами в массиве. В игре порядок создания меняется от чего угодно: от порядка
// загрузки уровня, от того, какой враг заспавнился первым, от пула объектов. Если он попадает в
// результат, то реплей, сетевой прогноз и golden-хеш разъезжаются на ровном месте — и разъезжаются
// НЕ воспроизводимо, что делает такой баг практически неотлавливаемым.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::physics;

uint64_t run_scene(const std::vector<BodyDesc>& descs) {
    World w(fixture::CAPACITY);
    fixture::fill(w, descs);
    fixture::run(w, fixture::STEPS);
    return w.hash();
}

// Перестановка детерминированная, а не `std::shuffle`: тест обязан падать одинаково у всех, иначе
// «у меня зелено» становится законным ответом на красный CI. Взят линейный конгруэнтный шаг по
// индексу — он перемешивает достаточно, чтобы сломать любую зависимость от порядка вставки.
void permute(const std::vector<BodyDesc>& in, std::vector<BodyDesc>& out, uint32_t seed) {
    out.clear();
    const uint32_t n = static_cast<uint32_t>(in.size());
    std::vector<bool> taken(n, false);
    uint32_t x = seed;
    for (uint32_t i = 0; i < n; ++i) {
        x = x * 1664525u + 1013904223u;
        uint32_t k = x % n;
        while (taken[k]) k = (k + 1) % n;
        taken[k] = true;
        out.push_back(in[k]);
    }
}

// Перестановка обязана быть НАСТОЯЩЕЙ и полной, и утверждать это надо явно. Сверять размеры
// бессмысленно — `permute` кладёт ровно n элементов по построению, и такая проверка не умеет
// падать. А вот тождественная перестановка сравнение хешей проходит сама собой, ничего не
// доказывая: гейт стоял бы на непроверенном свойстве генератора при конкретном n, и молча
// деградировал бы от любой правки состава сцены.
bool keeps_every_body(const std::vector<BodyDesc>& in, const std::vector<BodyDesc>& out) {
    if (in.size() != out.size()) return false;
    std::vector<uint32_t> ka, kb;
    for (const BodyDesc& d : in) ka.push_back(d.key);
    for (const BodyDesc& d : out) kb.push_back(d.key);
    std::sort(ka.begin(), ka.end());
    std::sort(kb.begin(), kb.end());
    return ka == kb;
}

bool order_differs(const std::vector<BodyDesc>& in, const std::vector<BodyDesc>& out) {
    for (size_t i = 0; i < in.size() && i < out.size(); ++i) {
        if (in[i].key != out[i].key) return true;
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics order gate\n");

    std::vector<BodyDesc> base;
    fixture::describe(base);
    const uint64_t reference = run_scene(base);

    // Позитивный контроль: сверка обязана уметь сказать «нет». Сцена, отличающаяся ОДНИМ телом,
    // сдвинутым на пиксель, даёт другой хеш — иначе сравнение хешей ниже ничего не доказывает,
    // и гейт зелен вакуумно.
    std::vector<BodyDesc> nudged = base;
    nudged.back().position.x = nudged.back().position.x + fix32::from_int(1);
    check(run_scene(nudged) != reference, "control: a different scene gives a different hash");

    std::vector<BodyDesc> shuffled;
    for (uint32_t seed = 1; seed <= 4; ++seed) {
        permute(base, shuffled, seed);
        check(keeps_every_body(base, shuffled), "permutation keeps every body exactly once");
        check(order_differs(base, shuffled), "permutation actually reorders the scene");
        check(run_scene(shuffled) == reference, "shuffled creation order gives the same hash");
    }

    // Дубль ключа — единственный вход, на котором независимость от порядка недостижима В ПРИНЦИПЕ:
    // равные ключи делают оба компаратора неполными, а порядок равных элементов introsort не
    // определяет, то есть его задаёт реализация стандартной библиотеки — то есть ОС. Замер до
    // фикса: одна и та же пара тел, добавленная в двух порядках, дала два разных хеша на ОДНОЙ
    // машине. Поэтому дубль обязан быть отбит на входе, а не всплыть красным CI через полгода.
    World w(fixture::CAPACITY);
    BodyDesc d;
    d.key = 7;
    d.shape = circle(fix32::from_int(4));
    check(w.add(d).valid(), "a fresh key is accepted");
    check(!w.add(d).valid(), "a duplicate key is refused");
    check(w.bodies().size() == 1, "the refused body never entered the world");

    std::printf("  reference = 0x%016llx\n", static_cast<unsigned long long>(reference));
    std::printf("framework-physics-order: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
