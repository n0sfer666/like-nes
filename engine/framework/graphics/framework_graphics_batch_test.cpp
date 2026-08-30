#include <cstdio>

#include "nine_slice.hpp"
#include "platform_args.hpp"
#include "sprite.hpp"

// Порядок отрисовки и батчи (спека #17, вертикаль 2, шаг A). Гейт спрашивает три разных вопроса:
//   * ПОРЯДОК — слой, потом материал, потом номер подачи, и порядок этот полный;
//   * ЧИСЛО БАТЧЕЙ — гейт 6 спеки: draw-call'ы считаются, а не оцениваются на глаз;
//   * РАСКЛАДКА 9-slice — внешняя граница панели равна заданной, углы держат размер.
// Голден сворачивает первые два в одно число: перестановка полей ключа переживает round-trip
// (порядок-то останется каким-то), а голден — нет.
//
// ОТКАЗЫ живут в отдельной цели `..._batch_refusal_test`, аллокации — в `..._batch_alloc_test`,
// по тому же основанию, что у шагов D и E: имя упавшей цели обязано называть класс поломки.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::graphics;

constexpr uint64_t GOLDEN = 0xbda7b692a87b1155ull;

constexpr uint32_t CAP = 512;
Sprite storage[CAP];
uint64_t keys[CAP];
Batch batches[CAP];

Sprite at(int32_t px, uint16_t material, int16_t layer) {
    Sprite s;
    s.center = {fix32::from_int(px), fix32::from_int(px * 2)};
    s.half = {fix32::from_int(4), fix32::from_int(4)};
    s.rgba = 0x102030ffu + static_cast<uint32_t>(px);
    s.region = static_cast<RegionId>(px & 0xff);
    s.material = material;
    s.layer = layer;
    return s;
}

void test_order() {
    SpriteList list(storage, keys, CAP);
    // Подача НАРОЧНО перемешана: список, поданный уже отсортированным, прошёл бы и без сортировки.
    list.push(at(1, 2, 1));
    list.push(at(2, 1, 1));
    list.push(at(3, 2, -1));
    list.push(at(4, 2, 1));
    list.push(at(5, 1, 0));
    check(list.count() == 5 && list.dropped() == 0, "every sprite is on the list");

    const uint32_t n = list.build(batches, CAP);
    // Слой -1 идёт первым, дальше слой 0, дальше слой 1 — и внутри слоя 1 материал 1 раньше
    // материала 2. Порядок подачи виден на двух спрайтах материала 2 в слое 1: 1 подан раньше 4.
    const int32_t want[5] = {3, 5, 2, 1, 4};
    for (uint32_t i = 0; i < 5; ++i) {
        const Sprite& s = list.drawn(i);
        check(s.center.x.raw == fix32::from_int(want[i]).raw, "draw order is layer, material, submission");
    }
    // Три батча, а не пять: материалы в порядке отрисовки идут 2, 1, 1, 2, 2 — и оба склеивания
    // здесь ПЕРЕСЕКАЮТ границу слоя. Это ровно то решение, ради которого батч рвётся по материалу,
    // а не по слою: граница слоя, за которой картинка не меняется, не стоит draw-call'а.
    check(n == 3, "batches break on material, not on every sprite and not on every layer");
    check(batches[0].material == 2 && batches[0].count == 1, "the first batch is the lowest layer");
    check(batches[1].material == 1 && batches[1].count == 2 && batches[1].first == 1,
          "one material spanning two layers is one draw call");
    check(batches[2].material == 2 && batches[2].count == 2 && batches[2].first == 3,
          "sprites sharing a material inside a layer share one draw call");
}

// Гейт 6 спеки: N спрайтов на M материалов дают ЧИСЛО draw-call'ов, а не «примерно столько».
void test_draw_calls() {
    SpriteList list(storage, keys, CAP);
    for (uint32_t i = 0; i < 300; ++i)
        list.push(at(static_cast<int32_t>(i), static_cast<uint16_t>(i % 4), 0));
    check(list.build(batches, CAP) == 4, "300 sprites over 4 materials cost 4 draw calls");

    // Тот же набор, разложенный по трём слоям. Границы слоёв склеиваются ТОЛЬКО там, где материал
    // совпал, поэтому вызовов двенадцать, а не четыре и не триста: слой есть ключ сортировки, а не
    // барьер батча, и число это обязано стоять утверждением.
    list.clear();
    for (uint32_t i = 0; i < 300; ++i)
        list.push(at(static_cast<int32_t>(i), static_cast<uint16_t>(i % 4),
                     static_cast<int16_t>(i % 3)));
    check(list.build(batches, CAP) == 12, "three layers over four materials cost twelve draw calls");

    // Вырожденный случай игры-образца: один атлас, один материал, один слой — ровно один вызов.
    list.clear();
    for (uint32_t i = 0; i < 300; ++i) list.push(at(static_cast<int32_t>(i), 7, 0));
    check(list.build(batches, CAP) == 1, "a single material is a single draw call");
}

void test_nine_slice() {
    SpriteList list(storage, keys, CAP);
    const NineSliceRegions r{1, 2, 3, 4, 5, 6, 7, 8, 9};
    const Vec2 center{fix32::from_int(100), fix32::from_int(50)};
    const Vec2 half{fix32::from_int(30), fix32::from_int(20)};
    nine_slice(list, r, center, half, {fix32::from_int(8), fix32::from_int(6)}, 0xffffffffu, 3, 2);
    check(list.count() == 9, "a panel wider than its corners is nine pieces");

    // Внешняя граница панели обязана совпасть с заданной: 9-slice растягивает середину, а не
    // раздувает прямоугольник. Проверяется по КРАЯМ кусков, а не по их центрам.
    fix32 left = list.data()[0].center.x, right = left, top = list.data()[0].center.y, bottom = top;
    for (uint32_t i = 0; i < list.count(); ++i) {
        const Sprite& s = list.data()[i];
        if ((s.center.x - s.half.x).raw < left.raw) left = s.center.x - s.half.x;
        if ((s.center.x + s.half.x).raw > right.raw) right = s.center.x + s.half.x;
        if ((s.center.y - s.half.y).raw < top.raw) top = s.center.y - s.half.y;
        if ((s.center.y + s.half.y).raw > bottom.raw) bottom = s.center.y + s.half.y;
    }
    check(left.raw == (center.x - half.x).raw && right.raw == (center.x + half.x).raw,
          "the panel is exactly as wide as asked");
    check(top.raw == (center.y - half.y).raw && bottom.raw == (center.y + half.y).raw,
          "the panel is exactly as tall as asked");
    check(list.data()[0].region == 1 && list.data()[4].region == 5 && list.data()[8].region == 9,
          "each cell draws the region its own corner names");
    check(list.data()[0].half.x.raw == fix32::from_int(4).raw &&
              list.data()[0].half.y.raw == fix32::from_int(3).raw,
          "a corner keeps its natural size");
    check(list.data()[4].half.x.raw == (half.x - fix32::from_int(8)).raw,
          "the middle column stretches by what the corners left");

    // Панель ровно в два угла шириной: середины нет, и это не потеря, а её отсутствие.
    list.clear();
    nine_slice(list, r, center, half, {half.x, half.y}, 0xffffffffu, 3, 2);
    check(list.count() == 4 && list.dropped() == 0, "a panel of pure corners is four pieces, not a loss");
}

uint64_t fold(const SpriteList& list, const Batch* b, uint32_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    const auto mix = [&h](uint64_t v) { h = (h ^ v) * 0x100000001b3ull; };
    for (uint32_t i = 0; i < n; ++i) {
        mix(b[i].first);
        mix(b[i].count);
        mix(b[i].material);
    }
    for (uint32_t i = 0; i < list.count(); ++i) {
        const Sprite& s = list.drawn(i);
        mix(static_cast<uint64_t>(static_cast<uint32_t>(s.center.x.raw)));
        mix(static_cast<uint64_t>(static_cast<uint32_t>(s.center.y.raw)));
        mix(static_cast<uint64_t>(static_cast<uint32_t>(s.half.x.raw)));
        mix(static_cast<uint64_t>(static_cast<uint32_t>(s.half.y.raw)));
        mix(s.rgba);
        mix(s.region);
        mix(s.material);
        mix(static_cast<uint64_t>(static_cast<uint32_t>(s.layer)));
    }
    return h;
}

// Сцена голдена собрана так, чтобы её задело ЛЮБОЕ из решений шага: три слоя (включая
// отрицательный), четыре материала, перемешанная подача и панель 9-slice поверх всего.
uint64_t golden_scene() {
    SpriteList list(storage, keys, CAP);
    for (uint32_t i = 0; i < 64; ++i)
        list.push(at(static_cast<int32_t>(i * 7 % 61), static_cast<uint16_t>(i % 4),
                     static_cast<int16_t>(static_cast<int>(i % 3) - 1)));
    nine_slice(list, NineSliceRegions{11, 12, 13, 14, 15, 16, 17, 18, 19},
               {fix32::from_int(160), fix32::from_int(90)}, {fix32::from_int(48), fix32::from_int(24)},
               {fix32::from_int(9), fix32::from_int(7)}, 0x88ccffaau, 9, 5);
    const uint32_t n = list.build(batches, CAP);
    std::printf("  batches = %u, sprites = %u\n", n, list.count());
    return fold(list, batches, n);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("sprite order and batching\n");

    test_order();
    test_draw_calls();
    test_nine_slice();

    const uint64_t h = golden_scene();
    std::printf("  sprite-batch hash = 0x%016llx\n", static_cast<unsigned long long>(h));
    check(h == GOLDEN, "the batch stream matches the golden");

    std::printf("framework-graphics-batch: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
