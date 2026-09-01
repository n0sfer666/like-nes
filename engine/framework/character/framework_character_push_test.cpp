#include <cstdio>

#include "framework_character_platform_scene.hpp"
#include "platform_args.hpp"

// Гейт СНОСА ДВИЖУЩИМСЯ ТЕЛОМ (`push.hpp`) — находка владельческого прогона §6 от 2026-09-01:
// «подпрыгнуть перед приближающейся платформой, и тебя как пасту из тюбика выжимает наверх».
//
// Своей целью рядом с гейтом переноса, а не случаем внутри него: раскладка общая (одна платформа,
// порядок кадра «мир, потом персонаж»), но персонаж здесь НЕ СТОИТ на платформе, и всё, что гейт
// переноса сверяет — путь пассажира с путём опоры, — тут неприменимо. Второй довод механический:
// `framework_character_platform_test.cpp` стоит на своём потолке бюджета длины, и случай, дописанный
// в него, пробил бы жёсткий порог.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::character;

fix32 fx(int v) { return fix32::from_int(v); }

// Невесомый профиль: падение сюда ничего не добавляет, а уносит — персонаж успевал бы выпасть из
// полосы платформы раньше, чем та до него доедет. Тяготение проверяется отдельным случаем ниже.
MoveProfile weightless() {
    MoveProfile p = default_profile();
    p.gravity_rise = fix32{};
    p.gravity_fall = fix32{};
    return p;
}

struct Trace {
    Vec2 moved;      // путь персонажа за прогон
    Vec2 platform;   // путь платформы за те же тики
    fix32 deepest;   // самое глубокое погружение в платформу, замеренное ПОСЛЕ тика персонажа
    fix32 rose;      // самый высокий подъём относительно старта
    uint32_t contacts = 0;  // тиков, в которых платформа реально въехала в персонажа
    bool crushed = false;
};

Trace hang_run(Stage& st, fix32 start_x, const MoveProfile& p, uint32_t ticks) {
    const MoveDerived d = derive(p, tick_dt());
    Character c;
    c.position = {start_x, HANG_Y};
    const Vec2 from = c.position;
    const Vec2 plat_from = st.world.body(st.platform).position;
    Trace r;
    for (uint32_t t = 0; t < ticks; ++t) {
        st.world.step(tick_dt());
        // Перекрытие меряется ДВАЖДЫ вокруг тика персонажа: до него — то, что создала платформа
        // (встреча вообще состоялась), после — то, что осталось неразобранным.
        const fix32 band = abs_fix(st.world.body(st.platform).position.y - c.position.y);
        const fix32 made = st.world.body(st.platform).position.x + PLATFORM_HALF_W -
                           (c.position.x - RIDER_HALF_W);
        if (made.raw > 0 && band < PLATFORM_HALF_H + RIDER_HALF_H) ++r.contacts;
        step(st.view(), make_rider(), p, d, MoveInput{}, tick_dt(), c);
        const fix32 into = st.world.body(st.platform).position.x + PLATFORM_HALF_W -
                           (c.position.x - RIDER_HALF_W);
        r.deepest = max_fix(r.deepest, into);
        r.rose = max_fix(r.rose, from.y - c.position.y);
        r.crushed = r.crushed || c.crushed;
    }
    r.moved = c.position - from;
    r.platform = st.world.body(st.platform).position - plat_from;
    return r;
}

// Платформа доезжает до висящего персонажа и толкает его перед собой. До сноса она проезжала СКВОЗЬ
// него: тик персонажа не смотрел на движущиеся тела вовсе, если не стоял на них.
void test_shoved_along_travel() {
    Stage st = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    const Trace r = hang_run(st, HANG_X, weightless(), 30);
    std::printf("  shove: char=(%.3f, %.3f) plat=%.3f deepest=%.3f\n", r.moved.x.to_double(),
                r.moved.y.to_double(), r.platform.x.to_double(), r.deepest.to_double());
    // Предпосылка: платформа реально доехала. Без неё «персонаж не утонул» было бы правдой и про
    // прогон, в котором встречи не случилось вовсе.
    check(fx(55) < r.platform.x, "precondition: the platform actually travels");
    check(fx(40) < r.moved.x, "a platform driving into a mid-air character shoves him along");
    // Замер ПИКОВЫЙ, а не конечный: выдавленное персонаж потом отыгрывает обратно.
    check(r.rose.raw == 0 && r.moved.y.raw == 0, "and never squeezes him across its travel");
    // Потолок — ОДИН ТИК хода платформы (2 юнита) с запасом: снос атомарен и случается в том же
    // тике, в котором возникло перекрытие, поэтому глубже хода за тик оно не бывает. До сноса тот
    // же замер давал полсотни юнитов — платформа заглатывала персонажа целиком.
    check(r.deepest < fx(3), "penetration never outruns a single tick of the platform's travel");
}

// Тяготение включено — то, что и наблюдал владелец: персонаж не висит, а падает мимо платформы.
// Утверждение сравнением с ПАРНЫМ прогоном, где платформа уезжает в другую сторону: падение обязано
// быть тем же самым. Односторонний порог («не поднялся») тут слабее — выдавливание вниз он бы
// пропустил, а именно вниз оно и уходило на голом воспроизведении.
void test_falling_past_is_undisturbed() {
    Stage met = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    const Trace r = hang_run(met, TOUCH_X, default_profile(), 30);
    Stage away = make_stage(physics::BodyType::Kinematic, {-CARRY_V, fix32{}}, false);
    const Trace f = hang_run(away, TOUCH_X, default_profile(), 30);
    std::printf("  fall:  met=%.3f free=%.3f rose=%.3f contacts=%u\n", r.moved.y.to_double(),
                f.moved.y.to_double(), r.rose.to_double(), r.contacts);
    check(f.contacts == 0, "precondition: the retreating platform never touches him");
    check(r.contacts >= 3, "precondition: the approaching one really does, and for several ticks");
    check(r.moved.y == f.moved.y, "being shoved sideways changes the fall by nothing at all");
    check(r.rose.raw == 0, "and never lifts him against gravity");
}

// Деваться некуда: за спиной стена. Персонаж останавливается у неё и объявляется вжатым — решать,
// что с этим делать, игре, а не контроллеру.
void test_pinned_against_a_wall() {
    Stage st = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    add_wall(st, HANG_X + RIDER_HALF_W + fx(6) + fx(8));
    const Trace r = hang_run(st, HANG_X, weightless(), 30);
    std::printf("  wall:  char=%.3f crushed=%d\n", r.moved.x.to_double(), r.crushed ? 1 : 0);
    check(r.crushed, "a character shoved into a wall is reported crushed");
    check(!(fx(6) < r.moved.x), "and is left standing at it, not pushed through");

    // ПАРА: та же платформа и тот же персонаж без стены. Без неё «вжат» краснел бы на каждой
    // встрече, и флаг сообщал бы про касание, а не про то, что деваться некуда.
    Stage open = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    const Trace o = hang_run(open, HANG_X, weightless(), 30);
    check(!o.crushed, "pair: the same shove with room to go never reports it");
}

// Неподвижное тело не сносит: снос читает ДВИЖЕНИЕ, а не запись в поле скорости. Пара к первому
// случаю — она же и контроль того, что снос не выдумывает ход там, где его нет.
void test_a_standing_body_shoves_nobody() {
    Stage plate = make_stage(physics::BodyType::Static, {CARRY_V, fix32{}}, false);
    Character c;
    c.position = {fix32{}, HANG_Y};   // прямо ВНУТРИ плиты: перекрытие есть, хода нет
    const MoveProfile p = weightless();
    const MoveDerived d = derive(p, tick_dt());
    const Vec2 from = c.position;
    for (uint32_t t = 0; t < 30; ++t) {
        plate.world.step(tick_dt());
        step(plate.view(), make_rider(), p, d, MoveInput{}, tick_dt(), c);
    }
    std::printf("  still: char=(%.3f, %.3f)\n", (c.position.x - from.x).to_double(),
                (c.position.y - from.y).to_double());
    check(plate.world.body(plate.platform).position.x.raw == 0,
          "precondition: a static body does not move, whatever it says");
    check(c.position.x == from.x, "a written-but-unmoving velocity shoves nobody sideways");
}

void test_a_bystander_never_hides_the_pusher() {
    Stage st = make_stage(physics::BodyType::Kinematic, {CARRY_V, fix32{}}, false);
    add_lid(st);
    // Предпосылка: крышка реально КАСАЕТСЯ персонажа на старте, и спрошенный свипом мир отвечает
    // именно ею. Без неё случай был бы вторым прогоном первого — с телом, до которого не дотянулись.
    physics::QueryFilter qf;
    physics::RayHit nearest;
    check(physics::shapecast(st.world, make_rider().shape, {HANG_X, HANG_Y}, fix32{}, {}, qf,
                             nearest) && nearest.key == 0,
          "precondition: the sweep really answers with the bystander, not the pusher");
    const Trace r = hang_run(st, HANG_X, weightless(), 30);
    std::printf("  lid:   char=(%.3f, %.3f) deepest=%.3f crushed=%d\n", r.moved.x.to_double(),
                r.moved.y.to_double(), r.deepest.to_double(), r.crushed ? 1 : 0);
    check(fx(40) < r.moved.x, "the pusher is found behind a body the character merely touches");
    // Крышка стоит ПОПЕРЁК сноса, и вжатым он от неё быть не может: вжимает только то, чья нормаль
    // смотрит навстречу. Иначе всякий, кого платформа везёт вдоль стены, объявлялся бы раздавленным.
    check(!r.crushed, "and a bystander across the shove is no reason to call him crushed");
}

// Замершее по правилу покоя тело не сносит: снос читает ДВИЖЕНИЕ, а `mutate` остров не будит
// намеренно (`world.hpp`). Случай отдельный от статики: там неподвижность по типу, здесь — по
// состоянию, и живёт она в другом ответе `support_velocity`.
void test_a_frozen_body_shoves_nobody() {
    Stage st = make_stage(physics::BodyType::Dynamic, {fix32{}, fix32{}}, false);
    for (uint32_t t = 0; t < 16; ++t) st.world.step(tick_dt());
    check(st.world.at_rest(st.platform), "precondition: an idle dynamic body really does freeze");

    // Скорость записана МЕЖДУ шагом мира и тиком персонажа — то окно, в котором `at_rest` ещё
    // говорит «замерло»: правило покоя сверит тело с копией лишь в начале следующего шага
    // (`world.hpp`), а `mutate` остров не будит. Тело не ехало, и снос по записанному числу увёл
    // бы персонажа от НЕПОДВИЖНОЙ платформы.
    const MoveProfile p = weightless();
    const MoveDerived d = derive(p, tick_dt());
    Character c;
    c.position = {TOUCH_X, HANG_Y};
    const fix32 plat = st.world.body(st.platform).position.x;
    st.world.mutate(st.platform).velocity = {CARRY_V, fix32{}};
    step(st.view(), make_rider(), p, d, MoveInput{}, tick_dt(), c);

    // ПАРА: то же тело, разбуженное явно. Без неё «замершее не сносит» было бы правдой и про сцену,
    // в которой снос сломан целиком.
    Stage awake = make_stage(physics::BodyType::Dynamic, {fix32{}, fix32{}}, false);
    for (uint32_t t = 0; t < 16; ++t) awake.world.step(tick_dt());
    awake.world.mutate(awake.platform).velocity = {CARRY_V, fix32{}};
    awake.world.wake(awake.platform);
    const Trace a = hang_run(awake, TOUCH_X, weightless(), 30);
    std::printf("  rest:  frozen=%.3f woken=%.3f plat=%.3f\n", (c.position.x - TOUCH_X).to_double(),
                a.moved.x.to_double(), a.platform.x.to_double());
    check(st.world.body(st.platform).position.x == plat && st.world.at_rest(st.platform),
          "precondition: the frozen body really stayed frozen and where it was");
    check(c.position.x == TOUCH_X, "a body frozen by the rest rule shoves nobody, whatever it says");
    check(fx(20) < a.moved.x, "pair: woken, the very same body travels and shoves him along");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character push gate\n");
    test_shoved_along_travel();
    test_falling_past_is_undisturbed();
    test_pinned_against_a_wall();
    test_a_standing_body_shoves_nobody();
    test_a_bystander_never_hides_the_pusher();
    test_a_frozen_body_shoves_nobody();
    std::printf("framework-character-push: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
