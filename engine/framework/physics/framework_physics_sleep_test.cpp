#include <cstdio>

#include "platform_args.hpp"
#include "world.hpp"

// Гейт 5 спеки #15: сон не влияет на результат — прогон со сном и без сна даёт одинаковый хеш.
//
// Сон в этом движке ровно одна вещь (`world.cpp`): для пары, обе стороны которой неподвижны,
// геометрия берётся из кеша вместо пересчёта узкой фазой. Значит гейт обязан сверять не только хеш
// состояния, но и ВСЁ, что мир показывает наружу через эту ветку, — поток событий и счётчики пар:
// расхождение в них состояние поменяет не сразу, а через кадр-другой, и хеш назовёт кадр, на
// котором проявилось, а не тот, на котором сломано.
//
// Сверка двух прогонов бессодержательна, пока не доказано, что ветка вообще сработала: мир, ни разу
// не заснувший, сверяет один и тот же код сам с собой. Позитивный контроль поэтому спрашивает не
// «замёрзли ли тела», а СРАБОТАЛА ЛИ ЭКОНОМИЯ: замирание — часть шага и живёт при выключенном сне
// тоже, так что счётчик замёрзших кадров остался бы прежним, заглуши хоть весь `recall` в
// `return false`. Спрашивается `recalled_pairs()`, и с обеих сторон: у спящего мира оно обязано быть
// ненулевым, у бодрствующего нулём, иначе выключатель ничего не выключает.
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

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);
constexpr fix32 HALF = fix32::from_int(8);
constexpr fix32 FLOOR_TOP = fix32::from_int(192);
constexpr uint32_t BOXES = 3;
// Окно на замирание с запасом: измерено 21 кадр на поставленную башню и 423 на пересыпание после
// удара. Окно — граница отказа, а не ожидание, поэтому взято на порядок шире измеренного.
constexpr uint32_t WINDOW = 30 * 60;
// Сколько держим замершую сцену. Именно здесь ветка сна и работает: пока башня стоит, со сном
// узкая фаза для её пар не считается вовсе.
constexpr uint32_t HOLD = 120;

// Пара миров на ОДНОЙ сцене: со сном и без. Расхождение фиксируется первым кадром, а не флагом, —
// «разошлись на 431-м» и «разошлись с самого начала» это разные поломки.
struct Twin {
    World sleepy{16};
    World awake{16};
    uint32_t frame = 0;
    uint32_t diverged = 0;
    uint32_t slept_frames = 0;
    // Накопительно за прогон: сколько пар шаг отдал из кеша вместо пересчёта. Второй — проверка
    // самого выключателя, а не симметрия ради симметрии.
    uint32_t recalled = 0;
    uint32_t awake_recalled = 0;
};

void build(World& w) {
    BodyDesc floor;
    floor.key = 1;
    floor.type = BodyType::Static;
    floor.shape = box(fix32::from_int(128), fix32::from_int(8));
    floor.position = {fix32{}, fix32::from_int(200)};
    floor.material = {fix32{}, fix32::from_float(0.6)};
    w.add(floor);

    for (uint32_t i = 0; i < BOXES; ++i) {
        BodyDesc b;
        b.key = 10 + i;
        b.shape = box(HALF, HALF);
        b.position = {fix32{}, FLOOR_TOP - HALF - fix32::from_int(static_cast<int32_t>(i) * 16)};
        b.mass = fix32::from_int(4);
        b.material = {fix32{}, fix32::from_float(0.6)};
        w.add(b);
    }
}

// Новичок ПОСЛЕ замирания: тело, добавленное в спящую сцену, обязано её разбудить. Двадцать юнитов
// над стопкой — падение на четверть секунды, за кадр меньше четверти собственной высоты.
void drop_in(World& w) {
    BodyDesc b;
    b.key = 20;
    b.shape = box(HALF, HALF);
    b.position = {fix32{}, FLOOR_TOP - HALF - fix32::from_int(BOXES * 16 + 20)};
    b.mass = fix32::from_int(4);
    b.material = {fix32{}, fix32::from_float(0.6)};
    w.add(b);
}

void start(Twin& t) {
    t.sleepy.set_sleep_enabled(true);
    t.awake.set_sleep_enabled(false);
    build(t.sleepy);
    build(t.awake);
}

bool tower_at_rest(const World& w) {
    for (uint32_t i = 0; i < BOXES; ++i) {
        if (!w.at_rest(BodyId{i + 1})) return false;
    }
    return true;
}

// Шаг обоих миров и сверка всего наблюдаемого. Правило покоя сверяется наравне с хешем: оно частью
// шага, а не сна, и выключатель не имеет права его сдвинуть.
void step(Twin& t) {
    t.sleepy.step(DT);
    t.awake.step(DT);
    ++t.frame;

    bool same = t.sleepy.hash() == t.awake.hash() &&
                t.sleepy.event_hash() == t.awake.event_hash() &&
                t.sleepy.contact_count() == t.awake.contact_count() &&
                t.sleepy.trigger_count() == t.awake.trigger_count();
    bool slept = false;
    for (uint32_t i = 0; i < static_cast<uint32_t>(t.sleepy.bodies().size()); ++i) {
        const BodyId id{i};
        same = same && t.sleepy.at_rest(id) == t.awake.at_rest(id);
        slept = slept || t.sleepy.sleeping(id);
    }
    t.recalled += t.sleepy.recalled_pairs();
    t.awake_recalled += t.awake.recalled_pairs();
    if (!same && t.diverged == 0) t.diverged = t.frame;
    if (slept) ++t.slept_frames;
}

// Кадров до замирания башни, 0 — не замерла в окне.
uint32_t run_until_rest(Twin& t) {
    for (uint32_t i = 0; i < WINDOW; ++i) {
        step(t);
        if (tower_at_rest(t.sleepy)) return i + 1;
    }
    return 0;
}

void test_sleep_is_invisible() {
    Twin t;
    start(t);

    const uint32_t froze = run_until_rest(t);
    check(froze != 0, "a placed tower freezes, so the sleeping branch has something to run on");
    for (uint32_t i = 0; i < HOLD; ++i) step(t);

    check(t.slept_frames != 0, "and the run with sleep on really does put bodies to sleep");
    // Ниже BOXES быть не может: замершая башня из трёх ящиков это три пары (пол-ящик и два
    // ящик-ящик), и держится она HOLD кадров. Порог, а не ноль, — иначе гейт зеленел бы на
    // единственном случайном срабатывании.
    check(t.recalled > BOXES, "and the sleeping branch really skipped narrowphase for whole pairs");
    check(t.awake_recalled == 0, "while the run with sleep off never skipped a single pair");
    // Замершая пара касаться не перестала: счётчик, падающий от замирания, сообщил бы игре о
    // расцеплении, которого не было, — и сообщил бы только в одном из двух миров.
    check(t.sleepy.contact_count() == BOXES, "contacts of a frozen island stay counted");

    drop_in(t.sleepy);
    drop_in(t.awake);
    uint32_t woke = 0;
    uint32_t refroze = 0;
    for (uint32_t i = 0; i < WINDOW && refroze == 0; ++i) {
        step(t);
        if (woke == 0 && !tower_at_rest(t.sleepy)) woke = i + 1;
        if (woke != 0 && tower_at_rest(t.sleepy) && t.sleepy.at_rest(BodyId{BOXES + 1})) {
            refroze = i + 1;
        }
    }
    std::printf("  froze at %u, newcomer woke it at +%u, refroze at +%u, slept %u frames, %u pairs "
                "recalled\n",
                froze, woke, refroze, t.slept_frames, t.recalled);
    check(woke != 0, "a newcomer landing on a sleeping stack wakes it");
    check(refroze != 0, "and the stack it landed on comes back to rest with it");
    check(t.sleepy.contact_count() == BOXES + 1, "with the new contact counted too");

    // Правка замершего через неконстантную ручку — на ПАРЕ миров: это единственный путь, на котором
    // игра трогает замершее, ничего не зная про сон. Что правка доезжает, а сама выдача ручки ничего
    // не двигает, доказывает гейт пробуждения; здесь вопрос один — влияет ли на это выключатель.
    for (uint32_t i = 0; i < HOLD; ++i) step(t);
    const BodyId top{BOXES};
    t.sleepy.body(top).velocity.x = fix32::from_int(60);
    t.awake.body(top).velocity.x = fix32::from_int(60);
    const fix32 before = t.sleepy.bodies()[BOXES].position.x;
    step(t);
    check(before < t.sleepy.bodies()[BOXES].position.x,
          "a push through a handle lands on the very next step with sleep on");
    for (uint32_t i = 0; i < WINDOW; ++i) step(t);

    // Главное утверждение гейта, и оно накопительное: расхождение хоть на одном из ~2000 кадров
    // назовёт номер кадра, а не факт.
    check(t.diverged == 0, "sleep changes neither state, nor events, nor contact counts, ever");
    if (t.diverged != 0) std::printf("  diverged at frame %u\n", t.diverged);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics sleep gate\n");
    test_sleep_is_invisible();
    std::printf("framework-physics-sleep: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
