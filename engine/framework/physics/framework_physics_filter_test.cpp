#include <cstdio>

#include "platform_args.hpp"
#include "world.hpp"

// Слои, маски и тела-триггеры. Правило здесь — ОТБРАСЫВАЮЩЕЕ, и проверяется оно только парами
// утверждений на ОДНОЙ И ТОЙ ЖЕ геометрии: сломанный фильтр не выдаёт неверное число, он выдаёт
// отсутствие контакта, а отсутствие само по себе неотличимо от «формы просто не пересеклись».
// Каждый случай ниже поэтому идёт вместе со своим позитивным двойником.
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

// Две коробки 16x16, перекрытые на 4 юнита по X. Гравитация выключена: вопрос ко всем случаям ниже
// один — доехала пара до узкой фазы или нет, — и падение тела примешало бы к нему второй.
struct Scene {
    World w{8};
    BodyDesc a;
    BodyDesc b;

    Scene() {
        w.set_gravity({fix32{}, fix32{}});
        a.key = 1;
        a.shape = box(fix32::from_int(8), fix32::from_int(8));
        b.key = 2;
        b.shape = box(fix32::from_int(8), fix32::from_int(8));
        b.position = {fix32::from_int(12), fix32{}};
    }

    void build() {
        w.add(a);
        w.add(b);
    }
};

void test_layers() {
    {
        // Позитивный контроль всего файла: те же две коробки с умолчаниями обязаны столкнуться и
        // разъехаться. Без него любая опечатка ниже давала бы вечно зелёный гейт.
        Scene s;
        s.build();
        s.w.step(fix32::from_float(1.0 / 60.0));
        check(s.w.contact_count() == 1, "default layers let the overlap through");
        check(s.w.bodies()[0].position.x.raw < 0, "and the solver actually pushes them apart");
    }
    {
        Scene s;
        s.a.layer = 1u;
        s.a.mask = 1u;
        s.b.layer = 2u;
        s.b.mask = 2u;
        s.build();
        s.w.step(fix32::from_float(1.0 / 60.0));
        check(s.w.contact_count() == 0, "disjoint layers drop the pair before narrowphase");
        check(s.w.bodies()[0].position.x.raw == 0, "and nothing moves the filtered body");
    }
    {
        // ОДНОСТОРОННЕЕ несогласие: `b` готов встретить `a`, `a` не готов встретить `b`. Правило из
        // одной проверки (`layer_a & mask_b`) пропустило бы эту пару — и ответ зависел бы от того,
        // чей ключ меньше, потому что порядок в паре нормализован ключом.
        Scene s;
        s.a.layer = 1u;
        s.a.mask = 1u;
        s.b.layer = 2u;
        s.b.mask = MASK_ALL;
        s.build();
        s.w.step(fix32::from_float(1.0 / 60.0));
        check(s.w.contact_count() == 0, "agreement is two-sided: one refusal drops the pair");
    }
    {
        // Зеркало предыдущего: несогласие переехало на другую сторону пары, ответ обязан не
        // измениться. Пара утверждений вместе доказывает симметрию правила, чего ни одно из них
        // поодиночке не доказывает.
        Scene s;
        s.a.layer = 1u;
        s.a.mask = MASK_ALL;
        s.b.layer = 2u;
        s.b.mask = 2u;
        s.build();
        s.w.step(fix32::from_float(1.0 / 60.0));
        check(s.w.contact_count() == 0, "and it stays two-sided with the sides swapped");
    }
    {
        // Нулевая маска — законное намерение «выключенный коллайдер», а не ошибка. Утверждение
        // фиксирует именно это: `make_body` не чинит её приведением к `MASK_ALL`.
        Scene s;
        s.a.mask = 0u;
        s.build();
        s.w.step(fix32::from_float(1.0 / 60.0));
        check(s.w.contact_count() == 0, "a zero mask means 'with nobody' and is kept as such");
    }
}

void test_triggers() {
    {
        Scene s;
        s.b.trigger = true;
        s.b.type = BodyType::Static;
        s.build();
        s.w.step(fix32::from_float(1.0 / 60.0));
        check(s.w.contact_count() == 0, "a trigger pair never reaches the solver");
        check(s.w.trigger_count() == 1, "but it is found and counted");
        check(s.w.bodies()[0].position.x.raw == 0, "and the trigger does not push the body");

        // Событие обязано нести геометрию, а не только факт: глубина 4 (коробки 16x16 на расстоянии
        // 12) и нормаль из `a` в `b`. Зона урона рисует искры там, где задело.
        check(s.w.events().size() == 1, "the overlap produces exactly one event");
        const ContactEvent& e = s.w.events()[0];
        check(e.trigger, "the event is marked as a trigger event");
        check(e.phase == ContactPhase::Begin, "and its first frame is a begin");
        check(e.normal == Vec2{fix32::from_int(1), fix32{}}, "event normal points a->b");
        check(abs_fix(e.penetration - fix32::from_int(4)) < fix32::from_float(0.05),
              "event penetration = 4");
    }
    {
        // Тот же кадр без флага: контакт обязан появиться, тело — сдвинуться. Пара доказывает, что
        // выше отработал флаг, а не что-то ещё в раскладке.
        Scene s;
        s.b.type = BodyType::Static;
        s.build();
        s.w.step(fix32::from_float(1.0 / 60.0));
        check(s.w.contact_count() == 1, "the same scene without the flag is a solved contact");
        check(s.w.trigger_count() == 0, "and produces no trigger overlap");
    }
    {
        // Кинематика в статическом триггере: НИ ОДНО из двух тел не отвечает на импульс, и прежнее
        // правило широкой фазы («обоих не сдвинуть») выбросило бы пару — то есть ровно тот случай,
        // ради которого триггеры и заводят: движущаяся платформа, наехавшая на выключатель.
        Scene s;
        s.a.type = BodyType::Kinematic;
        s.b.type = BodyType::Static;
        s.b.trigger = true;
        s.build();
        s.w.step(fix32::from_float(1.0 / 60.0));
        check(s.w.trigger_count() == 1, "kinematic vs static trigger still reports an overlap");
    }
    {
        // А два статических тела в перекрытии события не дают: они не сдвинутся никогда, и поток
        // одинаковых событий до конца игры был бы шумом. Негативный двойник предыдущего случая —
        // вместе они показывают, что отсечка смотрит на подвижность, а не на тип вообще.
        Scene s;
        s.a.type = BodyType::Static;
        s.b.type = BodyType::Static;
        s.b.trigger = true;
        s.build();
        s.w.step(fix32::from_float(1.0 / 60.0));
        check(s.w.trigger_count() == 0, "two immovable bodies produce no per-frame trigger event");
    }
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics filter gate\n");
    test_layers();
    test_triggers();
    std::printf("framework-physics-filter: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
