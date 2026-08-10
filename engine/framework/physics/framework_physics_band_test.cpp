#include <cstdio>

#include "narrowphase.hpp"
#include "platform_args.hpp"
#include "units.hpp"

// Спекулятивная полоса как ОТДЕЛЬНЫЙ контракт: контакт живёт в `[-SPECULATIVE_MARGIN, +CONTACT_SLOP]`
// вокруг точного касания, и «почти коснулись» — это уже касание с ОТРИЦАТЕЛЬНОЙ глубиной.
//
// Отдельная цель, потому что полоса — не свойство одного пути узкой фазы, а одно число, продетое
// сквозь ВСЕ три пути и через два разных входа: шаг зовёт узкую фазу с полем, запросы и трассировка —
// с нулём. Сломанное сравнение с полем даёт не «неверную геометрию», а пропавшую опору: пара не
// родилась — значит нет ни контакта, ни события, ни счётчика, и в сцене это неотличимо от «ящик
// просто падает». Ловить такое сценой значит ловить последствие через шесть слоёв.
//
// Границы берутся РОВНО на концах полосы и по разряду вокруг них: голден из середины диапазона слеп
// к дефекту на границе, а полоса — это ровно граница.
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

constexpr fix32 ONE_RAW = fix32::from_raw(1);
constexpr fix32 HALF = fix32::from_int(8);
constexpr fix32 RADIUS = fix32::from_int(5);
constexpr fix32 REACH = fix32::from_int(6);

// Край полосы — ЛИТЕРАЛ, а не `SPECULATIVE_MARGIN`, и это не дублирование. Считай тест край из той
// же константы, которую проверяет, — и сужение поля сдвинуло бы вместе и вопрос, и ответ: раскладки
// уехали бы вслед за полем, все пробы остались бы внутри него, и гейт был бы зелен при ЛЮБОЙ ширине,
// включая нулевую (проверено инъекцией — так и было). Совпадение литерала с константой поэтому само
// становится утверждением; `units.hpp` объявляет полосу числом, 1/16 юнита.
constexpr fix32 BAND = fix32::from_float(1.0 / 16.0);

// Старшие биты идентификатора называют путь, которым точка посчитана (`contact.hpp`). Отсечение
// граней своего бита не имеет — у него их НЕТ ни одного, и это тоже опознаваемый ответ, а не «не
// знаю»: сравнение по маске отличает «пришло отсечением» от «пришло зазором ядер».
constexpr uint32_t PATH_BITS = POINT_CORE_ID_BIT | CORE_GAP_ID_BIT | REF_AXIS_ID_BIT;
constexpr uint32_t FACE_CLIP = 0;

// Пара форм, разнесённых вдоль +X ровно на заданный ЗАЗОР между поверхностями. Раскладка осевая
// намеренно: зазор обязан быть точным числом, иначе проверка «ровно на поле» мерила бы округление
// подготовки сцены, а не сравнение с полем.
struct Apart {
    WorldShape a;
    WorldShape b;
    Vec2 ca;
    Vec2 cb;
};

Apart lay_out(const Shape& sa, const Shape& sb, fix32 reach_a, fix32 reach_b, fix32 gap) {
    Apart p;
    p.ca = {fix32{}, fix32{}};
    p.cb = {reach_a + reach_b + gap, fix32{}};
    to_world(sa, p.ca, rotation(fix32{}), p.a);
    to_world(sb, p.cb, rotation(fix32{}), p.b);
    return p;
}

// Один вопрос полосы: при зазоре `gap` пара обязана дать контакт, его глубина обязана быть РОВНО
// минус зазором, и посчитать его обязан заявленный путь. Не «примерно»: глубина спекулятивной точки
// — это то, из чего решатель считает разрешённое сближение, и потерянный на ней разряд уезжает в
// скорость делением на dt.
void probe(const char* who, const Shape& sa, const Shape& sb, fix32 reach_a, fix32 reach_b,
           fix32 gap, bool expect, uint32_t path) {
    const Apart p = lay_out(sa, sb, reach_a, reach_b, gap);
    Manifold m;
    const bool hit = collide_shapes(p.a, p.ca, p.b, p.cb, SPECULATIVE_MARGIN, m);
    if (hit != expect) {
        std::printf("  FAIL: %s at gap %d/65536 reported %d, expected %d\n", who,
                    static_cast<int>(gap.raw), hit ? 1 : 0, expect ? 1 : 0);
        ++fails;
        return;
    }
    if (!expect) return;
    if (m.points[0].penetration.raw != -gap.raw) {
        std::printf("  FAIL: %s at gap %d/65536 reports depth %d, not minus the gap\n", who,
                    static_cast<int>(gap.raw), static_cast<int>(m.points[0].penetration.raw));
        ++fails;
    }
    if ((m.points[0].id & PATH_BITS) != path) {
        std::printf("  FAIL: %s at gap %d/65536 came from another narrowphase path (id %08x)\n", who,
                    static_cast<int>(gap.raw), static_cast<unsigned>(m.points[0].id));
        ++fails;
    }
}

// Полоса на одной паре форм, все четыре интересные точки сразу: точное касание, разряд внутри поля,
// РОВНО поле, разряд за полем. Ровно поле входит в полосу — сравнение в узкой фазе одностороннее
// (`margin < separation` отбрасывает), и закрытость интервала объявлена в `units.hpp`.
void walk_band(const char* who, const Shape& sa, const Shape& sb, fix32 reach_a, fix32 reach_b,
               uint32_t path) {
    probe(who, sa, sb, reach_a, reach_b, fix32{}, true, path);
    probe(who, sa, sb, reach_a, reach_b, BAND - ONE_RAW, true, path);
    probe(who, sa, sb, reach_a, reach_b, BAND, true, path);
    probe(who, sa, sb, reach_a, reach_b, BAND + ONE_RAW, false, path);
}

void test_band_on_every_path() {
    check(SPECULATIVE_MARGIN.raw == BAND.raw, "the speculative band is the 1/16 unit units.hpp says");
    check(CONTACT_SLOP.raw == BAND.raw,
          "and it is the same 1/16 the contact slop declares, which is what makes the band symmetric");

    // Формы приводятся `sanitize` — тем же, чем их приводит `make_body`. Без него нормали граней
    // пустые: их считает именно приведение, и путь отсечения работал бы по нулевым осям.
    const Shape bx = sanitize(box(HALF, HALF));
    const Shape ci = sanitize(circle(RADIUS));
    // Отрезок с радиусом: у пары КОЛЛИНЕАРНЫХ отрезков площади нет ни у одной формы, разделение
    // вдоль общей прямой ловит не SAT, а отдельная ось (`segments_apart`) — и дальше пара уходит на
    // зазор ядер. Единственная из четырёх раскладок, доходящая до этого пути.
    const Shape sg = sanitize(capsule({-REACH, fix32{}}, {REACH, fix32{}}, fix32::from_int(2)));

    // Точечное ядро: путь выбирается по `count == 1`, а не по имени формы, — поэтому спрошены оба
    // случая, круг против многоугольника и круг против круга.
    walk_band("point core", ci, bx, RADIUS, HALF, POINT_CORE_ID_BIT);
    walk_band("two cores", ci, ci, RADIUS, RADIUS, POINT_CORE_ID_BIT);
    // Отсечение граней ловит и ЗАЗОР: со спекулятивным полем точка отсечения принимается по зазору
    // до поля, поэтому осевая пара коробок остаётся на этом пути по всей полосе, а не сваливается на
    // зазор ядер, как было бы без поля.
    walk_band("face clip", bx, bx, HALF, HALF, FACE_CLIP);
    walk_band("core gap", sg, sg, REACH + fix32::from_int(2), REACH + fix32::from_int(2),
              CORE_GAP_ID_BIT);
}

// Перекрытие — вторая половина полосы, и знак здесь несущий: та же пара, вдвинутая внутрь, обязана
// дать ПОЛОЖИТЕЛЬНУЮ глубину. Без этого случая тест доказывал бы только то, что полоса ловит зазоры,
// и молчал бы о том, что она не съела обычный контакт.
void test_overlap_keeps_its_sign() {
    const Shape bx = sanitize(box(HALF, HALF));
    const fix32 deep = fix32::from_int(1);
    const Apart p = lay_out(bx, bx, HALF, HALF, -deep);

    Manifold m;
    check(collide_shapes(p.a, p.ca, p.b, p.cb, SPECULATIVE_MARGIN, m),
          "a real overlap is still a contact once the speculative band exists");
    check(m.count == 2, "and a face-on-face overlap still gives both of its points");
    check(m.points[0].penetration.raw == deep.raw, "with a positive depth, not a negated gap");
    check((m.points[0].id & PATH_BITS) == FACE_CLIP,
          "and it comes from the face-clipping path all the same");
}

// Вход запросов: `overlap` и трассировка зовут ту же узкую фазу с НУЛЕВЫМ полем, потому что отвечают
// на вопрос «кто здесь сейчас», а не «кого мы коснёмся за кадр». Ноль обязан быть точным контрактом:
// касание ровно в ноль — ещё контакт, разряд зазора — уже нет. Разъедься эти два входа, и `overlap`
// возвращал бы тела, стоящие рядом, а свип тормозил бы стрелу на 1/16 юнита раньше стены.
void test_query_entry_has_no_band() {
    const Shape bx = sanitize(box(HALF, HALF));
    Manifold m;

    const Apart touch = lay_out(bx, bx, HALF, HALF, fix32{});
    check(collide_shapes(touch.a, touch.ca, touch.b, touch.cb, fix32{}, m),
          "exact touching is a contact even with no speculative margin");

    const Apart sliver = lay_out(bx, bx, HALF, HALF, ONE_RAW);
    check(!collide_shapes(sliver.a, sliver.ca, sliver.b, sliver.cb, fix32{}, m),
          "and one raw unit of gap is not, which is what a query must answer");

    // Та же щель СО спекулятивным полем — контакт. Пара стоит рядом намеренно: она и есть
    // доказательство того, что расхождение двух входов реально, а не заявлено.
    check(collide_shapes(sliver.a, sliver.ca, sliver.b, sliver.cb, SPECULATIVE_MARGIN, m),
          "while the step, looking a frame ahead, does call the same sliver a contact");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics speculative band gate\n");
    test_band_on_every_path();
    test_overlap_keeps_its_sign();
    test_query_entry_has_no_band();
    std::printf("framework-physics-band: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
