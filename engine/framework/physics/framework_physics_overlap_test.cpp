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

// `each_contact` — тот же вопрос «кто здесь», но ОБХОДОМ и без вектора: спрашивают его из тика
// персонажа (`character/push.hpp`), которому нельзя в кучу. Отвечать одним ближайшим этот вопрос не
// умеет — на нулевом пути ближайших столько же, сколько касаний, и ничью между ними разводит ключ,
// то есть РАСКЛАДКА, — поэтому гейт стоит ровно на том, что обход отдаёт ВСЕХ.
struct Seen {
    uint32_t count = 0;
    uint32_t key[4] = {0, 0, 0, 0};
    Vec2 normal[4];
};

void note(void* user, const RayHit& hit) {
    Seen& s = *static_cast<Seen*>(user);
    if (s.count < 4) {
        s.key[s.count] = hit.key;
        s.normal[s.count] = hit.normal;
    }
    ++s.count;
}

// Ищется по КЛЮЧУ, а не по номеру вызова: порядок обхода — порядок полосы, и гейт, опершийся на
// него, поймал бы сам себя на первой же перетасованной раскладке (гейт 2 спеки #15).
bool seen_key(const Seen& s, uint32_t key, Vec2& normal) {
    for (uint32_t i = 0; i < s.count && i < 4; ++i) {
        if (s.key[i] != key) continue;
        normal = s.normal[i];
        return true;
    }
    return false;
}

void test_each_contact() {
    World w(8);
    w.add(wall(10, fix32::from_int(-4)));
    w.add(wall(20, fix32::from_int(4)));

    QueryFilter f;
    // Зонд мельче тел и стоит между ними: обе восьмёрки его накрывают, а отведённый влево он
    // остаётся наедине с одной.
    const Shape probe = box(fix32::from_int(2), fix32::from_int(2));

    Seen both;
    each_contact(w, probe, {fix32{}, fix32{}}, fix32{}, f, &note, &both);
    check(both.count == 2, "the walk hands over every body the shape touches, not just the nearest");
    Vec2 left;
    Vec2 right;
    check(seen_key(both, 10, left) && seen_key(both, 20, right),
          "and names them by key, whatever order the band ran in");
    // Нормаль — та же, по которой персонажа разбирает скольжение: НАРУЖУ из тела, то есть в сторону
    // зонда. Без неё обход отвечал бы «касание есть», и снос не знал бы, куда сносить.
    check(left.x.raw > 0 && right.x.raw < 0, "each contact carries the normal out of its body");

    // Тот же критерий касания, что у свипа. Заявление несущее: персонаж ходит в мир свипами, и
    // обход, считающий касание по-своему, разошёлся бы с ними на допуске — а на нём стоит зазор SKIN.
    RayHit nearest;
    check(shapecast(w, probe, {fix32{}, fix32{}}, fix32{}, Vec2{}, f, nearest),
          "precondition: the sweep sees a contact in the same spot");
    Vec2 same;
    check(seen_key(both, nearest.key, same), "the sweep's answer is one of the walk's contacts");

    Seen alone;
    each_contact(w, probe, {fix32::from_int(-10), fix32{}}, fix32{}, f, &note, &alone);
    check(alone.count == 1 && alone.key[0] == 10, "moved inside one body it hands over that one");

    // Пусто — это НОЛЬ вызовов, а не последний ответ: обход ничего не накапливает и очищать ему
    // нечего, поэтому единственный способ сказать «никого» — не позвать ни разу.
    Seen nobody;
    each_contact(w, probe, {fix32::from_int(1000), fix32{}}, fix32{}, f, &note, &nobody);
    check(nobody.count == 0, "over empty space it calls back not once");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics overlap gate\n");
    test_overlap();
    test_each_contact();
    std::printf("framework-physics-overlap: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
