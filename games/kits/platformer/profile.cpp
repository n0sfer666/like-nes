#include "profile.hpp"

#include "units.hpp"

namespace framework::character {
namespace {

// sqrt(2*g*h) в Q16.16 БЕЗ промежуточного fix32. Произведение здесь переполняет диапазон по
// построению — при штатных 1200 юнит/с² и высоте 64 юнита это 153600, впятеро выше насыщения, —
// поэтому оно живёт в int64 ровно до корня. Тот же приём, что в `length()`: raw обоих множителей
// это величина, умноженная на 2^16, их произведение — на 2^32, и корень из него сразу даёт raw
// результата, без единого сдвига. Ноль и отрицательный вход дают ноль: высота не бывает
// отрицательной, а профиль с нулевой высотой прыжка — законный способ сказать «не прыгает».
fix32 rise_speed(fix32 g, fix32 h) {
    const int64_t two_gh = 2 * static_cast<int64_t>(g.raw) * h.raw;
    if (two_gh <= 0) return fix32{};
    return fix32::from_raw(fix32::sat(isqrt64(two_gh)));
}

// Поправка на ПОЛТИКА — то, чем обещание «вершина равна `jump_height`» отличается от приближения.
//
// Тик считает так: сначала тяготение меняет скорость, потом прыжок при необходимости её
// перезаписывает, и только потом позиция сдвигается на скорость. Значит на k-м тике интегрируется
// u_k = v0 - (k-1)*g*dt, и высота выходит H_k = dt*(k*v0 - g*dt*k*(k-1)/2). Непрерывная парабола в
// момент t = k*dt даёт v0*t - g*t^2/2 = dt*k*v0 - g*dt^2*k^2/2. Разница — ровно g*dt^2*k/2, и она
// РАСТЁТ с номером тика: без поправки персонаж улетает тем выше, чем дольше поднимается.
//
// Подстановка v0' = v0 - g*dt/2 сокращает её тождественно: H_k = k*dt*v0 - g*dt^2*k^2/2, то есть
// выборки ЛОЖАТСЯ НА параболу, а не рядом с ней. Остаётся только то, что вершина параболы почти
// никогда не приходится на границу тика: ближайшая выборка отстоит от неё не больше чем на
// полтика, а парабола за полтика опускается на g*dt^2/8. Это и есть весь недобор высоты — при 1200
// юнит/с² и 60 Гц он равен 0.042 юнита, то есть одной двадцатой пикселя.
fix32 half_tick_drop(fix32 g, fix32 dt) { return (g * dt) / fix32::from_int(2); }

} // namespace

MoveProfile sanitize(const MoveProfile& p) {
    MoveProfile o = p;
    o.max_speed = clamp_fix(p.max_speed, fix32{}, MAX_MOVE_SPEED);
    o.ground_accel = clamp_fix(p.ground_accel, fix32{}, MAX_MOVE_ACCEL);
    o.ground_decel = clamp_fix(p.ground_decel, fix32{}, MAX_MOVE_ACCEL);
    o.air_accel = clamp_fix(p.air_accel, fix32{}, MAX_MOVE_ACCEL);
    o.air_decel = clamp_fix(p.air_decel, fix32{}, MAX_MOVE_ACCEL);
    o.gravity_rise = clamp_fix(p.gravity_rise, fix32{}, MAX_MOVE_ACCEL);
    o.gravity_fall = clamp_fix(p.gravity_fall, fix32{}, MAX_MOVE_ACCEL);
    o.max_fall_speed = clamp_fix(p.max_fall_speed, fix32{}, MAX_MOVE_SPEED);
    o.jump_height = clamp_fix(p.jump_height, fix32{}, MAX_JUMP_HEIGHT);
    // Минимальная высота приводится к ЦЕЛЕВОЙ, а не к своему потолку: «отпустил сразу — прыгнул
    // выше, чем удерживая» это не экзотическая настройка, а перевёрнутая механика, и молча
    // разрешать её значит отдавать отладку такого профиля игре.
    o.min_jump_height = clamp_fix(p.min_jump_height, fix32{}, o.jump_height);
    o.corner_correction = clamp_fix(p.corner_correction, fix32{}, MAX_CORNER_CORRECTION);
    o.ground_snap = clamp_fix(p.ground_snap, fix32{}, MAX_GROUND_SNAP);
    o.max_slope = clamp_fix(p.max_slope, fix32{}, MAX_SLOPE);
    o.climb_speed = clamp_fix(p.climb_speed, fix32{}, MAX_MOVE_SPEED);
    o.coyote_ticks = p.coyote_ticks < MAX_WINDOW_TICKS ? p.coyote_ticks : MAX_WINDOW_TICKS;
    o.buffer_ticks = p.buffer_ticks < MAX_WINDOW_TICKS ? p.buffer_ticks : MAX_WINDOW_TICKS;
    o.ladder_regrab_ticks =
        p.ladder_regrab_ticks < MAX_WINDOW_TICKS ? p.ladder_regrab_ticks : MAX_WINDOW_TICKS;
    return o;
}

MoveDerived derive(const MoveProfile& p, fix32 dt) {
    const MoveProfile s = sanitize(p);
    const fix32 drop = half_tick_drop(s.gravity_rise, dt);
    MoveDerived d;
    // Поправка вычитается с полом в нуле: при вырожденно крупном шаге или вырожденно низком прыжке
    // она превышает саму скорость, и «отрицательная скорость подъёма» была бы прыжком ВНИЗ.
    d.jump_speed = max_fix(rise_speed(s.gravity_rise, s.jump_height) - drop, fix32{});
    d.min_jump_speed = max_fix(rise_speed(s.gravity_rise, s.min_jump_height) - drop, fix32{});
    d.jump_speed = min_fix(d.jump_speed, MAX_MOVE_SPEED);
    d.min_jump_speed = min_fix(d.min_jump_speed, d.jump_speed);
    return d;
}

MoveProfile default_profile() {
    MoveProfile p;
    p.max_speed = fix32::from_int(340);
    p.ground_accel = fix32::from_int(2400);
    p.ground_decel = fix32::from_int(3200);
    p.air_accel = fix32::from_int(1600);
    p.air_decel = fix32::from_int(900);
    p.gravity_rise = physics::DEFAULT_GRAVITY_Y;
    // Падение тяжелее подъёма вдвое — самая известная настройка платформера: симметричная парабола
    // читается как «залипание» в верхней точке.
    p.gravity_fall = physics::DEFAULT_GRAVITY_Y * fix32::from_int(2);
    p.max_fall_speed = fix32::from_int(900);
    p.jump_height = fix32::from_int(64);
    p.min_jump_height = fix32::from_int(16);
    p.coyote_ticks = 6;
    p.buffer_ticks = 6;
    // Четверть тайла вбок и полтайла вниз. Сдвиг мельче тайла по построению: приём обязан прощать
    // ЗАДЕТЫЙ угол, а не переносить персонажа в соседний проход. Притяжение вдвое больше сдвига,
    // потому что мерит другое — высоту ступеньки под ногами, а не ширину промаха.
    p.corner_correction = fix32::from_int(4);
    p.ground_snap = fix32::from_int(8);
    // Ровно 45°: тайл-склон вертикали 3 — единственная наклонная грань, которую умеет выложить
    // карта, и профиль по умолчанию обязан пускать персонажа по ней ходить. Проверяется РАВЕНСТВОМ
    // (`slide.hpp`), поэтому запаса «чуть больше единицы» тут не нужно.
    p.max_slope = fix32::from_int(1);
    // Лазание втрое медленнее бега: по лестнице поднимаются, а не взбегают, и 120 юнит/с это семь с
    // половиной тайлов в секунду. Окно перехвата — восемь тиков, чуть больше окна прощения: за
    // шесть тиков прыжок с лестницы не успевает вынести ноги за её пределы, и персонаж хватался бы
    // обратно, не покинув тайла.
    p.climb_speed = fix32::from_int(120);
    p.ladder_regrab_ticks = 8;
    return sanitize(p);
}

} // namespace framework::character
