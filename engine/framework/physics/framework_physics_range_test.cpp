#include <cstdio>

#include "platform_args.hpp"
#include "world.hpp"

// Гейт на ЗАЯВЛЕННЫЙ диапазон входов — и он появился не из аккуратности, а из уже случившегося.
//
// Гейты 1 и 2 гоняют сцену из середины диапазона: масса 2 и 4 при потолке 4096, скорость 40 при
// потолке 2048. Ровно поэтому они пропустили дефект, живший в дереве: импульс считался делением на
// сумму обратных масс в Q16.16, частное насыщалось, накопленная сумма переставала расти — и тело
// массой 243 и тяжелее ПРОВАЛИВАЛОСЬ сквозь статику, при том что кламп массы сам же объявлял
// легальным весь диапазон до 4096. Golden при этом оставался зелёным: сцена аккуратно обходила
// отказ. Здесь проверяются ГРАНИЦЫ того, что модуль о себе объявил, а не удобная середина.
//
// Проверяются ПРОГОНАМИ: тело роняется и смотрится, где оно остановилось. Клампы, приводящие вход
// в диапазон, живут отдельной целью (`framework_physics_clamp_test`) — там нет ни одного шага мира.
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

constexpr fix32 FLOOR_Y = fix32::from_int(200);
constexpr fix32 FLOOR_HALF_Y = fix32::from_int(8);
constexpr fix32 BOX_HALF = fix32::from_int(16);

fix32 dt() { return fix32::from_int(1) / fix32::from_int(60); }

// Параметры прогона отдельной структурой, а не шестью позиционными аргументами: у половины из них
// одинаковый тип fix32, и перепутанная пара на вызове не даёт ни ошибки компиляции, ни падения —
// она даёт молча другой замер.
struct Case {
    fix32 mass = fix32::from_int(4);
    Shape shape = box(BOX_HALF, BOX_HALF);
    fix32 speed;                            // стартовая скорость по Y
    fix32 angle;                            // стартовый поворот, в оборотах
    fix32 floor_half_y = FLOOR_HALF_Y;
    fix32 gravity_y = DEFAULT_GRAVITY_Y;
};

// Пол-статика и одно падающее на него тело. Возвращает ТЕЛО целиком: низ AABB отвечает на вопрос про
// контакт, угол — на вопрос про вращение, и оба вопроса задаются одному и тому же прогону.
Body drop(const Case& c) {
    World w(8);
    w.set_gravity({fix32{}, c.gravity_y});

    BodyDesc floor;
    floor.key = 1;
    floor.type = BodyType::Static;
    floor.shape = box(fix32::from_int(256), c.floor_half_y);
    floor.position = {fix32{}, FLOOR_Y};
    w.add(floor);

    BodyDesc b;
    b.key = 2;
    b.shape = c.shape;
    b.position = {fix32{}, fix32{}};
    b.velocity = {fix32{}, c.speed};
    b.angle = c.angle;
    b.mass = c.mass;
    w.add(b);

    for (uint32_t i = 0; i < 240; ++i) w.step(dt());
    return w.bodies()[1];
}

// Низ берётся из AABB, а не из полувысоты формы: полувысоты у формы больше нет, а у повёрнутой её и
// не было бы. С +Y вниз низ — это `max.y`.
fix32 drop_bottom(const Case& c) { return bounds(drop(c)).max.y; }

// Тело лежит на полу, если его низ у верхней грани: глубже — решатель не удержал контакт, выше —
// зависло в воздухе. Допуск в 3 юнита покрывает разрешённое проникновение (CONTACT_SLOP) и
// последний шаг интеграции.
bool rests(fix32 bottom) {
    const fix32 top = FLOOR_Y - FLOOR_HALF_Y;
    return abs_fix(bottom - top) < fix32::from_int(3);
}

void test_mass_range() {
    // Границы клампа и оба берега значения, на котором прежняя арифметика ломалась.
    const fix32 masses[] = {MIN_MASS,          fix32::from_int(1),   fix32::from_int(242),
                            fix32::from_int(243), fix32::from_int(1024), MAX_MASS};
    for (fix32 m : masses) {
        Case c;
        c.mass = m;
        const fix32 bottom = drop_bottom(c);
        if (!rests(bottom)) {
            std::printf("  mass %d -> bottom %d (floor top %d)\n", m.to_int(), bottom.to_int(),
                        (FLOOR_Y - FLOOR_HALF_Y).to_int());
        }
        check(rests(bottom), "a body of any declared mass rests on static geometry");
    }
}

void test_speed_range() {
    // Замер, а не гипотеза: MAX_SPEED — потолок АРИФМЕТИКИ, и он НЕ является безопасной скоростью.
    // Окно корректного разрешения для этой пары — сумма полуразмеров по оси движения (8 + 16 = 24),
    // то есть 1440 юнит/с при dt = 1/60. Проверка обязана начинаться с того, что контракт это
    // признаёт: если бы `max_safe_speed` возвращал что-то выше MAX_SPEED, дальше проверялся бы
    // удачный подбор чисел, а не заявленная граница.
    const fix32 safe = max_safe_speed(dt(), FLOOR_HALF_Y + BOX_HALF);
    check(safe < MAX_SPEED, "the arithmetic ceiling is NOT a guarantee against tunnelling");

    // Гравитация выключена намеренно: она разогнала бы тело выше проверяемой скорости за первые же
    // кадры, и прогон перестал бы отвечать на заданный вопрос.
    Case ok;
    ok.speed = safe - fix32::from_int(64);
    ok.gravity_y = fix32{};
    check(rests(drop_bottom(ok)), "below the documented window the body stops at the floor");

    // Контроль на ТОЙ ЖЕ геометрии: проверка обязана уметь увидеть проскок, иначе «остановился»
    // подписывалось бы и под прогоном, который ничего не наблюдает. Выше окна ось наименьшего
    // перекрытия выбирает сторону по центрам и выталкивает тело наружу вниз — непрерывное
    // обнаружение спека #15 выносит за скоуп явно.
    Case fast = ok;
    fast.speed = MAX_SPEED;
    check(!rests(drop_bottom(fast)),
          "control: above it the body goes through, and the check sees that");
}

// Вращение — вторая половина того же вопроса о диапазоне масс, и молчала она по той же причине, что
// и первая: наблюдения не было. Решатель хранил ОБРАТНЫЙ момент инерции, а обратная величина в
// Q16.16 упирается в младший разряд снизу: у коробки 32x32 он равнялся raw 383 при массе 1, единице
// при 256 и РОВНО НУЛЮ при 384 — тяжелее этого тело переставало вращаться совсем, тихо и глубоко
// внутри диапазона, объявленного законным. Гейт 1 этого не видел: его сцена вращения не наблюдает.
void test_rotation_range() {
    constexpr fix32 START = turns_from_degrees(15);
    const fix32 masses[] = {fix32::from_int(1), fix32::from_int(383), fix32::from_int(384),
                            fix32::from_int(1024), MAX_MASS};
    for (fix32 m : masses) {
        Case c;
        c.mass = m;
        c.angle = START;
        // Наклонённая коробка, упав на пол, обязана завалиться на грань: контакт приходит в угол, и
        // плечо от центра масс до него создаёт момент. Порог в 0.01 оборота (3.6 градуса) — заметно
        // меньше ожидаемых 15 и заметно больше дрожания в покое.
        const Body settled = drop(c);
        const fix32 turned = abs_fix(settled.angle - START);
        const fix32 bottom = bounds(settled).max.y;
        if (!(fix32::from_float(0.01) < turned) || !rests(bottom)) {
            std::printf("  mass %d -> turned %d/1000 of a turn, bottom %d (floor top %d)\n",
                        m.to_int(), (turned * fix32::from_int(1000)).to_int(), bottom.to_int(),
                        (FLOOR_Y - FLOOR_HALF_Y).to_int());
        }
        check(fix32::from_float(0.01) < turned, "a body of any declared mass still rotates");
        // Поворот сам по себе ещё не поведение: тело, провалившееся сквозь пол, крутится по дороге
        // вниз свободно и утверждение выше проходит с запасом. Вопрос «завалилось на грань» имеет
        // смысл только вместе с «и осталось на полу», поэтому оба стоят на ОДНОМ прогоне.
        check(rests(bottom), "and it does so while staying on the floor");
    }
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics range gate\n");
    test_mass_range();
    test_speed_range();
    test_rotation_range();
    std::printf("framework-physics-range: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
