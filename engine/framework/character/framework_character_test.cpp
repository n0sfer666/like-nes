#include <cstdio>

#include "controller.hpp"
#include "framework_character_scene.hpp"
#include "platform_args.hpp"
#include "trajectory.hpp"

// Гейт 1 спеки #16: записанный ввод даёт ту же траекторию побитово — на macOS, Linux и Windows.
//
// Ввод записан ЗДЕСЬ, а не прочитан из файла: сценарий гейта обязан меняться вместе с голденом, в
// одном коммите, а файл рядом с тестом позволяет их разъехать и оставляет вопрос «голден чего».
//
// Сценарий подобран так, чтобы в результате участвовал КАЖДЫЙ механизм вертикали 1: разгон и
// торможение по земле, разворот на месте, прыжок с удержанием, прыжок с ранним отпусканием, сход с
// края (окно coyote), удар о стену и удар о потолок. Прогон, где работает только тяготение, дал бы
// хеш, устойчивый к поломке контроллера, — ровно то, чем плох голден из середины диапазона.
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

constexpr uint32_t TICKS = 600;

// Голден траектории. Перепечатывается ОСОЗНАННО и только вместе с объяснением, что именно в
// движении изменилось: несовпадение здесь означает либо расхождение платформ, либо правку
// контроллера — и различить их может только тот, кто её делал.
//
// Перепечатан 2026-08-23 вместе с фиксом нормали свипа на вырожденном касании (`cast.cpp`,
// `terminal_normal`), и вот что изменилось в движении. На фазе «разбег влево до стены и в неё»
// персонаж встаёт во ВНУТРЕННИЙ УГОЛ сцены: левая грань корпуса в зазоре `SKIN` от стены, нижняя —
// в таком же зазоре над полом, то есть нижний-левый угол корпуса стоит ровно по диагонали от
// верхнего-левого угла пола. Свидетель расстояния сцеплял на этой раскладке УГОЛ с УГЛОМ и отдавал
// свипу по полу нормаль (0.707, -0.707) вместо (0, -1) — хотя грани перекрываются во всю ширину
// корпуса, а найденное перебором расстояние 0.177 даже не минимальное (по вертикали 0.125).
// Отход на зазор идёт ВДОЛЬ нормали, поэтому диагональ толкала персонажа не только вверх, но и
// вправо, ОТ стены: на один тик `hit_wall` падал в ноль, тело поднималось на 0.055 юнита над полом
// и уносило эту высоту до конца фазы. Теперь нормаль спрашивается у узкой фазы, ответ (0, -1),
// персонаж упирается в стену и стоит на полу.
//
// Расхождение старой и новой траектории замерено и ограничено: тики 196..309 из 600, дальше
// бит-в-бит совпадение; максимум сдвига 3625 raw по Y (0.055 юнита) и 2394 по X; флаги разошлись
// на ОДНОМ тике — том самом отлипании от стены.
constexpr uint64_t GOLDEN = 0xb8a964e87679bce4ull;

// Тик, на котором сценарий нажимает прыжок после схода с края. Это ИЗМЕРЕННАЯ величина, а не
// подобранная: тик схода зависит от разгона, то есть от профиля, и назван здесь одним числом ровно
// затем, чтобы правка профиля роняла голден вместе с этим гейтом, а не тихо превращала прыжок в
// окне прощения в обычное падение.
constexpr uint32_t COYOTE_PRESS = 421;
constexpr uint32_t VOID_PRESS = 500;

fix32 tick_dt() { return fix32::from_int(1) / fix32::from_int(60); }

// Ввод на тик `t`. Фазы идут подряд и каждая длиннее, чем нужно на её механизм: короткая фаза,
// сдвинувшаяся на тик при правке соседней, ломала бы голден, ничего не сообщая.
MoveInput scripted(uint32_t t) {
    MoveInput in;
    if (t < 30) {
        in.move_x = fix32::from_int(1);            // разгон вправо
    } else if (t < 60) {
        in.move_x = fix32::from_int(-1);           // разворот на месте: торможение, а не разгон
    } else if (t < 100) {
        in.jump_held = t < 90;                     // прыжок с удержанием, на месте
    } else if (t < 140) {
        in.jump_held = t == 100;                   // прыжок с ранним отпусканием
    } else if (t < 250) {
        in.move_x = fix32::from_int(-1);           // разбег влево до стены и в неё
    } else if (t < 310) {
        in.move_x = fix32::from_int(-1);
        in.jump_held = t < 300;                    // прыжок у стены под потолком: удар головой
    } else if (t < COYOTE_PRESS) {
        in.move_x = fix32::from_int(1);            // разбег вправо к краю площадки и сход с него
    } else if (t < COYOTE_PRESS + 20) {
        in.move_x = fix32::from_int(1);
        in.jump_held = true;                       // прыжок в окне coyote, уже в воздухе
    } else if (t < VOID_PRESS) {
        in.move_x = fix32::from_int(1);            // падение в пропасть за краем площадки
    } else {
        // Нажатие В ПУСТОТЕ: опоры под персонажем нет и не будет, прыжок не состоится, а буфер
        // обязан взвестись и истечь. Без этой фазы `buffer_left` не наблюдается в прогоне ни разу —
        // все прыжки выше срабатывают в том же тике, в котором нажаты, и гасят буфер на шаге 4.
        in.move_x = fix32::from_int(1);
        in.jump_held = t < VOID_PRESS + 2;
    }
    return in;
}

void test_golden() {
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    physics::World w = make_scene();
    const CharacterHull hull = make_hull();
    Character c = standing_at(fix32::from_int(-100));

    TrajectoryHash h;
    bool saw_ground = false, saw_air = false, saw_wall = false, saw_ceiling = false;
    bool saw_rise = false, saw_coyote = false, saw_buffer = false, saw_coyote_jump = false;
    for (uint32_t t = 0; t < TICKS; ++t) {
        // Окно, ОТКРЫТОЕ до шага, и подъём ПОСЛЕ него — это и есть «прыжок случился из окна».
        // Одного `coyote_left > 0` мало: оно доказывает, что окно открылось, а не что сценарий в
        // него попал. Тик нажатия зависит от разгона, то есть от профиля, и уехав из окна, он
        // оставил бы гейт зелёным при неработающем прощении.
        const bool armed = !c.on_ground && c.coyote_left > 0;
        step(w, hull, p, d, scripted(t), tick_dt(), c);
        saw_coyote_jump = saw_coyote_jump || (armed && c.velocity.y.raw < 0);
        h.feed(c);
        saw_ground = saw_ground || c.state == MoveState::Ground;
        saw_air = saw_air || c.state == MoveState::Air;
        saw_wall = saw_wall || c.hit_wall;
        saw_ceiling = saw_ceiling || c.hit_ceiling;
        saw_rise = saw_rise || c.velocity.y.raw < 0;
        saw_coyote = saw_coyote || c.coyote_left > 0;
        saw_buffer = saw_buffer || c.buffer_left > 0;
    }

    std::printf("  character trajectory hash = 0x%016llx\n",
                static_cast<unsigned long long>(h.value));

    // Голден без утверждения о СОДЕРЖАНИИ прогона проверяет только повторяемость: контроллер,
    // который ничего не делает, тоже даёт один и тот же хеш на трёх ОС. Поэтому сценарий обязан
    // доказать, что каждый механизм в нём сработал, — и доказать ЭТИМ прогоном, а не соседним.
    check(saw_ground && saw_air, "the run visits both movement states");
    check(saw_rise, "the run contains a jump");
    check(saw_wall, "the run hits a wall");
    check(saw_ceiling, "the run hits the ceiling");
    check(saw_coyote, "the run opens the coyote window");
    check(saw_coyote_jump, "and the run actually jumps from inside it");
    check(saw_buffer, "the run arms the jump buffer");
    check(h.value == GOLDEN, "the trajectory matches the golden");
}

void test_determinism_is_input_only() {
    // Тот же ввод из ДРУГОГО стартового объекта даёт тот же хеш. Ловит состояние, утёкшее в
    // статику или в мир: `World` здесь константный для контроллера, но пересоздаётся всё равно —
    // общий мир между двумя прогонами скрыл бы ровно такую утечку.
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    uint64_t seen[2] = {0, 0};
    for (int run = 0; run < 2; ++run) {
        physics::World w = make_scene();
        const CharacterHull hull = make_hull();
        Character c = standing_at(fix32::from_int(-100));
        TrajectoryHash h;
        for (uint32_t t = 0; t < TICKS; ++t) {
            step(w, hull, p, d, scripted(t), tick_dt(), c);
            h.feed(c);
        }
        seen[run] = h.value;
    }
    check(seen[0] == seen[1], "two runs of the same input agree");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character golden gate\n");
    test_golden();
    test_determinism_is_input_only();
    std::printf("framework-character: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
