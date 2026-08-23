#include <cstdio>

#include "controller.hpp"
#include "framework_character_scene.hpp"
#include "platform_args.hpp"

// Гейт 2 спеки #16: окна прощения срабатывают РОВНО в заявленном окне тиков — на границе и за ней.
//
// Табличный, а не «прыгнул после схода — значит coyote работает»: окно, укоротившееся или
// удлинившееся на тик, срабатывает ровно так же в середине диапазона и молчит. Проверяются обе
// стороны границы, потому что односторонняя проверка («в окне сработало») пропускает окно длиной в
// вечность, а обратная («за окном не сработало») — окно длиной в ноль.
//
// Отсчёт ведётся от НАБЛЮДАЕМОГО события, а не от номера тика в прогоне: тик, на котором персонаж
// сходит с края, зависит от разгона, то есть от профиля, и зашитая константа превратила бы гейт
// окна в гейт скорости бега.
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

fix32 tick_dt() { return fix32::from_int(1) / fix32::from_int(60); }

bool rising(const Character& c) { return c.velocity.y.raw < 0; }

// Сбежать с края площадки и попробовать прыгнуть через `delay` тиков после потери опоры.
// `delay == 0` означало бы прыжок в тике схода, когда опора ещё есть, — это обычный прыжок, а не
// прощение, поэтому отсчёт начинается с единицы.
bool coyote_jump(const MoveProfile& p, const MoveDerived& d, uint32_t delay) {
    physics::World w = make_scene();
    const CharacterHull hull = make_hull();
    Character c = standing_at(LEDGE_RIGHT - fix32::from_int(60));
    uint32_t since_air = 0;
    bool airborne = false;
    for (uint32_t t = 0; t < 240; ++t) {
        MoveInput in;
        in.move_x = fix32::from_int(1);
        in.jump_held = airborne && since_air == delay;
        step(w, hull, p, d, in, tick_dt(), c);
        if (in.jump_held) return rising(c);
        if (airborne) ++since_air;
        if (!airborne && !c.on_ground) {
            airborne = true;
            since_air = 1;
        }
    }
    return false;
}

// Тик, на котором прыжок после падения становится возможен: опора появилась в конце предыдущего
// тика, и шаг 4 контроллера видит её только в следующем.
uint32_t landing_tick(const MoveProfile& p, const MoveDerived& d) {
    physics::World w = make_scene();
    const CharacterHull hull = make_hull();
    Character c = standing_at(fix32{});
    c.position.y = c.position.y - fix32::from_int(120);
    c.on_ground = false;
    c.state = MoveState::Air;
    for (uint32_t t = 0; t < 240; ++t) {
        step(w, hull, p, d, MoveInput{}, tick_dt(), c);
        if (c.on_ground) return t + 1;
    }
    return 0;
}

// Нажать за `ahead` тиков до первого тика с опорой и проверить, дожило ли нажатие до него.
bool buffered_jump(const MoveProfile& p, const MoveDerived& d, uint32_t landing, uint32_t ahead) {
    physics::World w = make_scene();
    const CharacterHull hull = make_hull();
    Character c = standing_at(fix32{});
    c.position.y = c.position.y - fix32::from_int(120);
    c.on_ground = false;
    c.state = MoveState::Air;
    for (uint32_t t = 0; t <= landing; ++t) {
        MoveInput in;
        in.jump_held = (t + ahead == landing);
        step(w, hull, p, d, in, tick_dt(), c);
        if (t == landing) return rising(c);
    }
    return false;
}

void test_coyote_window() {
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    check(coyote_jump(p, d, 1), "the coyote window opens on the tick after leaving the ledge");
    check(coyote_jump(p, d, p.coyote_ticks), "the coyote window is still open on its last tick");
    check(!coyote_jump(p, d, p.coyote_ticks + 1), "the coyote window is shut one tick later");
}

void test_buffer_window() {
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    const uint32_t landing = landing_tick(p, d);
    std::printf("  landing tick=%u\n", landing);
    check(landing > 0, "the fall reaches the floor within the run");
    check(buffered_jump(p, d, landing, 0), "a press on the landing tick jumps");
    check(buffered_jump(p, d, landing, p.buffer_ticks), "a press buffer_ticks early survives");
    check(!buffered_jump(p, d, landing, p.buffer_ticks + 1), "a press one tick earlier expires");
}

// Высота прыжка, СРАБОТАВШЕГО ИЗ БУФЕРА: нажатие живёт `ahead` тиков и доезжает до опоры уже
// отпущенным. Отсчёт вершины ведётся от позиции в тике приземления.
fix32 buffered_apex(const MoveProfile& p, const MoveDerived& d, uint32_t landing, uint32_t ahead,
                    bool keep_held) {
    physics::World w = make_scene();
    const CharacterHull hull = make_hull();
    Character c = standing_at(fix32{});
    c.position.y = c.position.y - fix32::from_int(120);
    c.on_ground = false;
    c.state = MoveState::Air;
    fix32 start_y{};
    fix32 top{};
    for (uint32_t t = 0; t < 400; ++t) {
        MoveInput in;
        in.jump_held = keep_held ? (landing <= t + ahead) : (t + ahead == landing);
        if (t == landing) {
            start_y = c.position.y;
            top = start_y;
        }
        step(w, hull, p, d, in, tick_dt(), c);
        if (t < landing) continue;
        top = min_fix(top, c.position.y);
        if (t > landing && c.on_ground) break;
    }
    return start_y - top;
}

// Буфер не отменяет ПЕРЕМЕННУЮ ВЫСОТУ. Случай нужен потому, что обрыв подъёма ловился ФРОНТОМ
// отпускания, а прыжок из буфера срабатывает в тике, где кнопка отпущена уже давно, — фронта на нём
// не случается никогда. Короткое касание, попавшее в буфер, поднимало персонажа на полную высоту:
// буфер молча выключал механику, ради которой заведена минимальная высота.
//
// Вершина сверяется ПОЛОСОЙ вокруг ожидаемой высоты, а не «меньше целевой»: односторонний порог
// пропустил бы и прыжок, обрезанный до нуля. Ожидание здесь — РОВНО `min_jump_height`, и это не
// совпадение: обрыв случается в том же тике, что и старт, поэтому первым же шагом интегрируется уже
// урезанная скорость, а она выведена так, чтобы дать ровно эту высоту. Касание с удержанием
// (`framework_character_jump_test`) поднимает выше на `jump_speed*dt` — там первый тик успевает
// проинтегрировать полную скорость до того, как отпускание становится наблюдаемым.
void test_buffered_tap_stays_short() {
    const MoveProfile p = default_profile();
    const MoveDerived d = derive(p, tick_dt());
    const uint32_t landing = landing_tick(p, d);
    const fix32 apex = buffered_apex(p, d, landing, p.buffer_ticks, false);
    const fix32 target = p.min_jump_height;
    const fix32 band = (p.gravity_rise * tick_dt() * tick_dt()) / fix32::from_int(8);
    std::printf("  buffered tap apex=%.4f target=%.4f full=%.4f\n", apex.to_double(),
                target.to_double(), p.jump_height.to_double());
    check(!(target < apex), "a buffered tap never rises past its own release height");
    check(target - band - fix32::from_float(0.01) < apex, "and it does reach that height");
    // Контроль: ТО ЖЕ нажатие, дожившее до опоры в буфере, но не отпущенное, обязано дать полную
    // высоту. Без него случай выше проверяет, что прыжок из буфера просто слаб, а не что его
    // обрезало отпускание.
    const fix32 held = buffered_apex(p, d, landing, p.buffer_ticks, true);
    check(p.jump_height - band - fix32::from_float(0.01) < held,
          "control: the same buffer held down still clears the full height");
}

void test_zero_windows_are_honest() {
    // Профиль без прощения обязан НЕ прощать. Без этого случая гейт выше проверяет только то, что
    // окна длиннее нуля: реализация, которая всегда разрешает прыжок, прошла бы обе границы.
    MoveProfile p = default_profile();
    p.coyote_ticks = 0;
    p.buffer_ticks = 0;
    p = sanitize(p);
    const MoveDerived d = derive(p, tick_dt());
    check(!coyote_jump(p, d, 1), "a zero coyote window forgives nothing");
    const uint32_t landing = landing_tick(p, d);
    check(buffered_jump(p, d, landing, 0), "a press on the landing tick still jumps without a buffer");
    check(!buffered_jump(p, d, landing, 1), "a zero buffer remembers nothing");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character window gate\n");
    test_coyote_window();
    test_buffer_window();
    test_buffered_tap_stays_short();
    test_zero_windows_are_honest();
    std::printf("framework-character-window: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
