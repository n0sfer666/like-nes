#include "controller.hpp"

#include "assist.hpp"

namespace framework::character {
namespace {

constexpr fix32 ONE = fix32::from_int(1);

// Подтянуть величину к цели не быстрее, чем `rate` за секунду, и НЕ ПЕРЕЛЕТЕТЬ. Перелёт здесь не
// косметика: скорость, проскочившая цель на кванте, на следующем тике возвращается назад, и
// персонаж на полном ходу дрожит вокруг максимальной скорости — в позиции это видно, а в профиле
// искать нечего.
fix32 approach(fix32 v, fix32 target, fix32 rate, fix32 dt) {
    const fix32 step_v = rate * dt;
    if (v < target) return min_fix(target, v + step_v);
    if (target < v) return max_fix(target, v - step_v);
    return v;
}

// Разгон или торможение. Разворот на месте считается ТОРМОЖЕНИЕМ, а не разгоном в новую сторону:
// иначе персонаж, бегущий вправо и получивший «влево», разворачивался бы тем резче, чем выше был
// разогнан, — то есть управление становилось бы отзывчивее на скорости, а не наоборот.
fix32 rate_for(fix32 v, fix32 target, fix32 accel, fix32 decel) {
    if (target.raw == 0) return decel;
    const bool same_dir = (target.raw > 0 && v.raw >= 0) || (target.raw < 0 && v.raw <= 0);
    return same_dir ? accel : decel;
}

} // namespace

void step(const CollisionScene& s, const CharacterHull& hull, const MoveProfile& raw,
          const MoveDerived& d, const MoveInput& in, fix32 dt, Character& c) {
    // Профиль приводится в диапазон ЗДЕСЬ, а не «предполагается приведённым». `derive` уже так и
    // делает, и расхождение между двумя входами одной подсистемы было бы ловушкой: профиль,
    // прошедший вывод, но не приведённый на тике, даёт отрицательное торможение — а `approach` с
    // отрицательным `rate` уводит скорость ОТ цели, то есть расходится. Тем же приведением
    // закрываются отрицательная скорость падения (вечный подъём) и максимум выше насыщения Q16.16.
    // Цена — десяток клампов против шейпкаста в том же тике.
    const MoveProfile p = sanitize(raw);
    const bool pressed = in.jump_held && !c.jump_was_held;
    // Опора СНИМАЕТСЯ до всех шагов: шаг 4 гасит `on_ground` сам, и читать её после него значит
    // читать уже переписанное значение. Держалось это на ручном `!jumped` в шаге 8 — то есть на
    // дисциплине, а снимок делает то же самое устройством.
    const bool was_ground = c.on_ground;

    // 1. Буфер помнит нажатие `buffer_ticks` тиков ПОСЛЕ того, в котором оно случилось, поэтому
    // заводится на единицу больше: сам тик нажатия расходуется на проверку в шаге 4.
    if (pressed) c.buffer_left = p.buffer_ticks + 1;

    // 2. Горизонталь.
    const fix32 target = clamp_fix(in.move_x, -ONE, ONE) * p.max_speed;
    const fix32 accel = c.on_ground ? p.ground_accel : p.air_accel;
    const fix32 decel = c.on_ground ? p.ground_decel : p.air_decel;
    c.velocity.x = approach(c.velocity.x, target, rate_for(c.velocity.x, target, accel, decel), dt);

    // 3. Тяготение. Ветка берётся по ЗНАКУ скорости до её изменения: «поднимаемся» это
    // velocity.y < 0, потому что +Y смотрит вниз (`units.hpp`).
    const fix32 g = c.velocity.y.raw < 0 ? p.gravity_rise : p.gravity_fall;
    c.velocity.y = min_fix(c.velocity.y + g * dt, p.max_fall_speed);

    // 4. Прыжок: разрешение даёт опора ИЛИ незакрытое окно coyote, запрос — живой буфер.
    const bool jumped = (c.on_ground || c.coyote_left > 0) && c.buffer_left > 0;
    if (jumped) {
        c.velocity.y = -d.jump_speed;
        c.buffer_left = 0;
        c.coyote_left = 0;
        c.jump_active = true;
        c.on_ground = false;
    }

    // 5. Отпускание обрывает подъём — но только СВОЙ подъём: `jump_active` гаснет на вершине и при
    // ударе о потолок, иначе отпускание кнопки во время падения с обрыва подрезало бы падение.
    //
    // Прыжок ИЗ БУФЕРА обрывается по самому факту ненажатой кнопки, а не по фронту отпускания. Он
    // срабатывает на тике, где кнопка уже отпущена, поэтому фронт `released` на нём не случается
    // НИКОГДА — и короткое касание, попавшее в буфер, поднимало персонажа на полную высоту, то есть
    // буфер молча отменял переменную высоту прыжка.
    const bool cut = jumped ? !in.jump_held : (!in.jump_held && c.jump_was_held);
    if (cut && c.jump_active && c.velocity.y.raw < 0) {
        c.velocity.y = max_fix(c.velocity.y, -d.min_jump_speed);
        c.jump_active = false;
    }
    if (c.velocity.y.raw >= 0) c.jump_active = false;

    // 6. Движение. Позиция клампится МИРОМ: за его границей Q16.16 насыщается, а насыщенная
    // координата делает мусором любой шейпкаст — персонаж, улетевший в пропасть, возвращался бы из
    // неё в неопределённом месте вместо того, чтобы честно упереться в край мира. Физика держит тот
    // же кламп на телах решателя (`world.cpp`), и держать его здесь по-другому было бы расхождением
    // двух путей движения в одном мире.
    //
    // Прощение угла потолка идёт ПЕРЕД движением, а не после разбора касания: сдвиг существует ровно
    // затем, чтобы удара головой не случилось вовсе. После касания скорость подъёма уже погашена
    // скольжением, и «поправленный» персонаж просто висел бы под потолком сдвинутым вбок.
    if (c.velocity.y.raw < 0)
        corner_correct(s, hull, c.velocity * dt, p.corner_correction, c.position);
    const SlideResult sr = move_and_slide(s, hull, c.velocity * dt, c.position, c.velocity);
    c.position.x = clamp_fix(c.position.x, -physics::WORLD_HALF, physics::WORLD_HALF);
    c.position.y = clamp_fix(c.position.y, -physics::WORLD_HALF, physics::WORLD_HALF);
    c.hit_ceiling = sr.hit_ceiling;
    c.hit_wall = sr.hit_wall;
    if (sr.hit_ceiling) c.jump_active = false;

    // 7. Истечение окон.
    if (c.coyote_left > 0) --c.coyote_left;
    if (c.buffer_left > 0) --c.buffer_left;

    // 8. Опора и состояние.
    c.on_ground = probe_ground(s, hull, c.position);
    // Прилипание к земле на спуске: опора БЫЛА, персонаж не поднимается, а пробы не хватило. Это и
    // есть спуск по ступеньке — без притяжения персонаж каждую ступеньку слетал бы в короткий полёт,
    // теряя и опору, и управление по земле.
    //
    // Прыжок сюда не попадает ПО ПОСТРОЕНИЮ, а не по списку исключений: его первый тик даёт
    // `velocity.y < 0`. Скорость падения при успехе гасится — притянутый персонаж СТОИТ, и оставить
    // ему накопленное падение значило бы отдать следующему тику скорость, которой не соответствует
    // ни одно движение.
    if (!c.on_ground && was_ground && c.velocity.y.raw >= 0 &&
        snap_to_ground(s, hull, p.ground_snap, c.position)) {
        c.on_ground = true;
        c.velocity.y = fix32{};
    }
    c.state = c.on_ground ? MoveState::Ground : MoveState::Air;
    if (c.on_ground) {
        c.coyote_left = 0;
        c.jump_active = false;
    } else if (was_ground && !jumped) {
        // Опора потеряна НЕ прыжком — значит персонаж сошёл с края, и это единственный случай, в
        // котором окно coyote имеет смысл. Прыжок опору тоже «теряет», и выдать окно там значило бы
        // разрешить второй прыжок в воздухе.
        c.coyote_left = p.coyote_ticks;
    }

    c.jump_was_held = in.jump_held;
}

} // namespace framework::character
