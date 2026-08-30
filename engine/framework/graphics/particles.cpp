#include "particles.hpp"

#include "fixtrig.hpp"

namespace framework::graphics {
namespace {

const fix32 ONE = fix32::from_int(1);
const fix32 TWO = fix32::from_int(2);

// Тот же линейный конгруэнтный генератор, что у частиц игры-образца (`example_ugly_game/fx.cpp`):
// решение владельца 1 просит обобщить найденное, а не переписать. Отличие ровно одно и несущее —
// число выходит `fix32` из СТАРШИХ разрядов состояния, а не `float` из младших: младшие разряды LCG
// худшие по периоду, а `float` не имел бы права попадать в sim-хеш.
fix32 next01(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return fix32::from_raw(static_cast<int32_t>(s >> 16));
}

int32_t clamp255(int32_t v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

uint32_t lerp_rgba(uint32_t a, uint32_t b, fix32 t) {
    uint32_t out = 0;
    for (int32_t i = 0; i < 4; ++i) {
        const int32_t sh = i * 8;
        const int32_t ca = static_cast<int32_t>((a >> sh) & 0xffu);
        const int32_t cb = static_cast<int32_t>((b >> sh) & 0xffu);
        const int64_t d = static_cast<int64_t>(cb - ca) * t.raw;
        out |= static_cast<uint32_t>(clamp255(ca + static_cast<int32_t>(d >> fix32::SHIFT))) << sh;
    }
    return out;
}

fix32 lerp_fix(fix32 a, fix32 b, fix32 t) { return a + (b - a) * t; }

fix32 floor_at(fix32 v, int32_t raw_min) {
    return v.raw < raw_min ? fix32::from_raw(raw_min) : v;
}

} // namespace

ParticleStore::ParticleStore(Particle* buf, uint32_t capacity, const EmitDesc* descs,
                             uint16_t desc_count, uint32_t seed)
    : buf_(buf),
      descs_(descs),
      // Ёмкость без буфера — ноль, а не заявленное число: тот же приём, что у списка спрайтов шага
      // A. Иначе первая же подача писала бы по нулевому указателю вместо честной потери. Таблицу
      // здесь НЕ проверяем: её отсутствие обнуляет `desc_count_` строкой ниже, и всякая подача
      // отбивается раньше, чем дойдёт до ёмкости, — вторая проверка была бы строкой, удаление
      // которой не замечает ни один гейт.
      capacity_(buf != nullptr ? capacity : 0),
      rng_(seed),
      desc_count_(descs != nullptr ? desc_count : 0) {}

void ParticleStore::clear() {
    count_ = 0;
    dropped_ = 0;
    accum_ = fix32{};
}

const Particle& ParticleStore::at(uint32_t i) const { return buf_[i]; }

uint32_t ParticleStore::burst(uint16_t desc, Vec2 at, uint32_t n) {
    // Описание вне таблицы и нулевое время жизни — ОТКАЗ, а не тихая частица: у первой не из чего
    // считать вид, вторая умрёт до первой отрисовки, и обе снаружи неотличимы от «не заказывали».
    if (desc >= desc_count_ || descs_[desc].life_ticks == 0) {
        dropped_ += n;
        return 0;
    }
    if (n > MAX_BURST) {
        dropped_ += n - MAX_BURST;
        n = MAX_BURST;
    }
    const EmitDesc& d = descs_[desc];
    uint32_t born = 0;
    for (uint32_t i = 0; i < n; ++i) {
        // Два числа на частицу, и берутся они ВСЕГДА — даже когда класть уже некуда.
        const fix32 turns = d.dir_turns + d.spread_turns * (next01(rng_) * TWO - ONE);
        const fix32 speed = lerp_fix(d.speed_min, d.speed_max, next01(rng_));
        // Третье и четвёртое — только заказавшему разброс: см. `EmitDesc::life_jitter`. Условие
        // стоит на ОПИСАНИИ, а не на частице, поэтому поток по-прежнему не зависит ни от заполнения
        // буфера, ни от того, какая частица по счёту.
        const fix32 lj = d.life_jitter.raw != 0 ? next01(rng_) : fix32{};
        const fix32 hj = d.half_jitter.raw != 0 ? next01(rng_) : fix32{};
        const fix32 sx = d.spawn_half.x.raw != 0 ? next01(rng_) * TWO - ONE : fix32{};
        const fix32 sy = d.spawn_half.y.raw != 0 ? next01(rng_) * TWO - ONE : fix32{};
        if (count_ >= capacity_) {
            ++dropped_;
            continue;
        }
        Particle& p = buf_[count_++];
        p.pos = {at.x + d.spawn_half.x * sx, at.y + d.spawn_half.y * sy};
        p.vel = {cos_turns(turns) * speed, sin_turns(turns) * speed};
        p.age = fix32{};
        // Пол у жизни ОДИН разряд, а не ноль: `draw` делит возраст на неё, и разброс, съевший жизнь
        // целиком, делил бы на ноль. Отказом это не сделано намеренно — `life_ticks == 0` отбивается
        // раньше и целым описанием, а здесь речь о крайнем броске уже принятого описания: убить
        // подачу из-за него значило бы, что редкая частица роняет весь взрыв.
        p.life = floor_at(fix32::from_int(d.life_ticks) * (ONE - d.life_jitter * lj), 1);
        p.scale = floor_at(ONE - d.half_jitter * hj, 0);
        p.desc = desc;
        ++born;
    }
    return born;
}

uint32_t ParticleStore::emit(uint16_t desc, Vec2 at, fix32 dt) {
    if (desc >= desc_count_) {
        ++dropped_;
        return 0;
    }
    accum_ = accum_ + descs_[desc].rate_per_tick * dt;
    const int32_t whole = accum_.to_int();
    if (whole <= 0) return 0;
    accum_ = accum_ - fix32::from_int(whole);
    return burst(desc, at, static_cast<uint32_t>(whole));
}

void ParticleStore::integrate(fix32 dt) {
    // Порядок внутри шага несущий и потому назван: сначала скорость получает тяготение и трение,
    // потом позиция едет УЖЕ НОВОЙ скоростью, потом стареет возраст. Переставь два первых — и
    // частица за первый свой шаг улетит без гравитации; переставь два последних — и она проживёт на
    // шаг дольше заказанного.
    uint32_t kept = 0;
    for (uint32_t i = 0; i < count_; ++i) {
        Particle p = buf_[i];
        const EmitDesc& d = descs_[p.desc];
        p.vel = (p.vel + d.gravity * dt) * (ONE - (ONE - d.damping) * dt);
        p.pos = p.pos + p.vel * dt;
        p.age = p.age + dt;
        if (!(p.age < p.life)) continue;
        // Уплотнение СДВИГОМ, а не обменом с последней. Игра-образец меняет местами
        // (`example_ugly_game/fx.cpp`), и для аддитивной вспышки это ничего не значит; здесь же
        // порядок подачи входит в ключ сортировки шага A, то есть обмен переставлял бы две
        // полупрозрачные частицы одного слоя между кадрами — ровно то мигание, которое спека
        // запрещает требованием стабильного порядка внутри слоя.
        buf_[kept++] = p;
    }
    count_ = kept;
}

uint32_t ParticleStore::draw(SpriteList& out) const {
    uint32_t emitted = 0;
    for (uint32_t i = 0; i < count_; ++i) {
        const Particle& p = buf_[i];
        const EmitDesc& d = descs_[p.desc];
        // Регион `0` означает «не рисовать» — то же соглашение, что у тайлов шага B: невидимая
        // частица иначе занимала бы слот батча прозрачным квадом.
        if (d.region == 0) continue;
        const fix32 t = p.age / p.life;
        const fix32 h = lerp_fix(d.half_start, d.half_end, t) * p.scale;
        Sprite s;
        s.center = p.pos;
        s.half = {h, h};
        s.rgba = lerp_rgba(d.rgba_start, d.rgba_end, t);
        s.region = d.region;
        s.material = d.material;
        s.layer = d.layer;
        out.push(s);
        ++emitted;
    }
    return emitted;
}

} // namespace framework::graphics
