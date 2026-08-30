#include <cstdio>

#include "nine_slice.hpp"
#include "platform_args.hpp"
#include "sprite.hpp"

// Отказы раскладки спрайтов (спека #17, вертикаль 2, шаг A). Отдельная цель по тому же основанию,
// что у шагов D и E: имя упавшей цели в логе CI обязано называть КЛАСС поломки, а «не поместилось»
// и «нарисовалось не в том порядке» — разные вопросы, и в одном файле второй тонет.
//
// Каждый случай здесь — недорисованный кадр, который снаружи выглядит ровно как задуманный.
// Единственная разница между «панель не влезла в буфер» и «панель такой и хотели» — счётчик,
// поэтому проверяется он, а не отсутствие падения.
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

Sprite one(uint16_t material, int16_t layer) {
    Sprite s;
    s.half = {fix32::from_int(1), fix32::from_int(1)};
    s.material = material;
    s.layer = layer;
    return s;
}

void test_sprite_overflow() {
    Sprite storage[4];
    uint64_t keys[4];
    SpriteList list(storage, keys, 4);
    for (uint32_t i = 0; i < 10; ++i) list.push(one(0, 0));
    check(list.count() == 4, "the list stops at its capacity");
    check(list.dropped() == 6, "every sprite past capacity is counted");
    // Принятые обязаны остаться целыми: буфер, затёртый одиннадцатым спрайтом по кругу, тоже
    // «не падает».
    Batch b[4];
    check(list.build(b, 4) == 1, "the sprites that did fit still batch");
}

void test_batch_overflow() {
    Sprite storage[8];
    uint64_t keys[8];
    SpriteList list(storage, keys, 8);
    for (uint16_t m = 0; m < 8; ++m) list.push(one(m, 0));
    Batch b[3];
    const uint32_t n = list.build(b, 3);
    check(n == 3, "the batch buffer stops at its capacity");
    // Пять спрайтов остались без вызова отрисовки — и это ПОТЕРЯ, а не «просто три батча».
    check(list.dropped() == 5, "sprites left without a draw call are counted");
    check(b[2].material == 2 && b[2].first == 2, "the batches that did fit are the first ones");
}

void test_no_batch_buffer() {
    Sprite storage[4];
    uint64_t keys[4];
    SpriteList list(storage, keys, 4);
    list.push(one(0, 0));
    list.push(one(1, 0));
    check(list.build(nullptr, 4) == 0, "no batch buffer is no batches");
    check(list.dropped() == 2, "with nowhere to put batches every sprite is a loss");
    list.clear();
    list.push(one(0, 0));
    Batch b[1];
    check(list.build(b, 0) == 0, "zero batch capacity is no batches");
    check(list.dropped() == 1, "zero batch capacity loses the sprites too");
}

void test_no_storage() {
    SpriteList list(nullptr, nullptr, 32);
    // Ёмкость заявлена, буферов нет — и список обязан считать это нулевой ёмкостью, а не писать по
    // нулевому указателю. Заявленное число тут страшнее отсутствующего.
    list.push(one(0, 0));
    check(list.count() == 0 && list.dropped() == 1, "a capacity without buffers is no capacity");
    Batch b[4];
    check(list.build(b, 4) == 0, "an empty list is no batches");

    Sprite storage[4];
    SpriteList half(storage, nullptr, 4);
    half.push(one(0, 0));
    check(half.count() == 0 && half.dropped() == 1, "half a pair of buffers is no capacity");
}

void test_empty_list() {
    Sprite storage[4];
    uint64_t keys[4];
    SpriteList list(storage, keys, 4);
    Batch b[4];
    check(list.build(b, 4) == 0, "nothing submitted is nothing drawn");
    check(list.dropped() == 0, "nothing submitted is nothing lost");
}

void test_nine_slice_refusals() {
    Sprite storage[16];
    uint64_t keys[16];
    SpriteList list(storage, keys, 16);
    const NineSliceRegions r{1, 2, 3, 4, 5, 6, 7, 8, 9};
    const Vec2 center{fix32::from_int(10), fix32::from_int(10)};
    const Vec2 half{fix32::from_int(6), fix32::from_int(4)};

    // Угол больше половины панели: урезается, и внешняя граница остаётся ЗАДАННОЙ. Без урезания два
    // угла перекрылись бы, и панель нарисовалась бы шире, чем просили, — молча.
    nine_slice(list, r, center, half, {fix32::from_int(50), fix32::from_int(50)}, 0xffu, 0, 0);
    check(list.count() == 4 && list.dropped() == 0, "an oversized corner is clamped, not lost");
    fix32 left = list.data()[0].center.x - list.data()[0].half.x;
    fix32 right = left;
    for (uint32_t i = 0; i < list.count(); ++i) {
        const Sprite& s = list.data()[i];
        if ((s.center.x - s.half.x).raw < left.raw) left = s.center.x - s.half.x;
        if ((s.center.x + s.half.x).raw > right.raw) right = s.center.x + s.half.x;
    }
    check(left.raw == (center.x - half.x).raw && right.raw == (center.x + half.x).raw,
          "a clamped panel is still exactly as wide as asked");

    // Отрицательный угол — не «зеркальный», а никакой: панель из одной середины. И середина эта
    // обязана быть РОВНО панелью: угол, не отсечённый в ноль, вычитается из середины со знаком
    // минус, то есть раздувает её, — а сам при этом не рисуется, потому что вырожден. Снаружи это
    // видно только по краям, и без утверждения о краях защиты у решения нет.
    list.clear();
    nine_slice(list, r, center, half, {fix32::from_int(-3), fix32::from_int(-3)}, 0xffu, 0, 0);
    check(list.count() == 1 && list.data()[0].region == 5, "a negative corner is no corner at all");
    check(list.data()[0].half.x.raw == half.x.raw && list.data()[0].half.y.raw == half.y.raw,
          "a panel of pure middle is exactly the panel, not wider");

    // Панель нулевого размера рисует ноль кусков и НИЧЕГО не теряет: рисовать там нечего, а не «не
    // поместилось». То же различение, что у рамки шага E.
    list.clear();
    nine_slice(list, r, center, {fix32{}, fix32{}}, {fix32::from_int(2), fix32::from_int(2)}, 0xffu,
               0, 0);
    check(list.count() == 0 && list.dropped() == 0, "a panel of zero size is nothing, not a loss");

    // Панель, не влезшая в список: куски считаются поштучно, как обычные спрайты.
    Sprite tiny[5];
    uint64_t tiny_keys[5];
    SpriteList small(tiny, tiny_keys, 5);
    nine_slice(small, r, center, half, {fix32::from_int(2), fix32::from_int(1)}, 0xffu, 0, 0);
    check(small.count() == 5 && small.dropped() == 4, "panel pieces past capacity are counted");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("sprite batching refusals\n");

    test_sprite_overflow();
    test_batch_overflow();
    test_no_batch_buffer();
    test_no_storage();
    test_empty_list();
    test_nine_slice_refusals();

    std::printf("framework-graphics-batch-refusal: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
