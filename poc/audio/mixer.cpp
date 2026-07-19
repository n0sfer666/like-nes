#include "mixer.hpp"

#include <cmath>

namespace audio {
namespace {

const fix32 HALF = fix32::from_float(0.5);
const fix32 ONE = fix32::from_int(1);
const fix32 DUCK_GAIN = fix32::from_float(0.35);   // music пригашается до 0.35 при ducking
const fix32 DUCK_STEP = fix32::from_float(0.0003); // рамп duck-огибающей ЗА СЕМПЛ (× frames)
                                                   // → скорость от sample-time, не от размера блока
const fix32 PAN_DIST = fix32::from_int(8);       // полу-ширина панорамы (мир→[-1,1])
const fix32 MAX_DIST = fix32::from_int(24);      // радиус слышимости (linear rolloff)
constexpr uint32_t BUS_N = static_cast<uint32_t>(Bus::Count);

fix32 approach(fix32 cur, fix32 target, fix32 step) {
    if (cur.raw < target.raw) { fix32 n = cur + step; return n.raw > target.raw ? target : n; }
    fix32 n = cur - step;
    return n.raw < target.raw ? target : n;
}

fix32 clamp_pan(fix32 p) {
    if (p.raw > ONE.raw) return ONE;
    if (p.raw < -ONE.raw) return fix32::from_raw(-ONE.raw);
    return p;
}

int16_t clamp16(int64_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return static_cast<int16_t>(v);
}

// Один семпл голоса + продвижение playhead. Стрим-underrun → тишина (не деактивирует).
int16_t fetch_advance(Voice& v, uint64_t& underruns) {
    if (v.ring) {
        if (v.ring->readable() == 0) { ++underruns; return 0; }
        return v.ring->pop_one();
    }
    if (v.frames == 0) { v.active = false; return 0; } // пустой резидент → не читаем pcm[0]
    if (v.playhead >= v.frames) {
        if (v.loop) v.playhead = 0;
        else { v.active = false; return 0; }
    }
    return v.pcm[v.playhead++];
}

} // namespace

Mixer::Mixer(Backend b) : backend_(b), master_(fix32::from_int(1)), duck_env_(fix32::from_int(1)) {
    for (uint32_t i = 0; i < BUS_N; ++i) bus_gain_[i] = fix32::from_int(1);
}

void Mixer::register_source(uint64_t guid, const int16_t* pcm, uint32_t frames, SampleRing* ring) {
    if (source_count_ >= MAX_SOURCES) return;
    sources_[source_count_++] = Source{guid, pcm, frames, ring};
}

const Source* Mixer::find_source(uint64_t guid) const {
    for (uint32_t i = 0; i < source_count_; ++i)
        if (sources_[i].guid == guid) return &sources_[i];
    return nullptr;
}

void Mixer::apply(const AudioCommand& c, uint32_t offset) {
    switch (static_cast<CmdType>(c.type)) {
        case CmdType::Play: {
            if (c.bus >= BUS_N) return; // невалидная шина → drop (иначе OOB busL[]/busR[])
            const Source* src = find_source(c.guid);
            if (!src) return; // отсутствует ассет → тишина-placeholder, не crash
            Voice& v = pool_.alloc(seq_++);
            v.id = c.voice_id;
            v.pcm = src->pcm; v.frames = src->frames; v.ring = src->ring;
            v.gain = fix32::from_raw(c.gain);
            v.x = fix32::from_raw(c.x); v.y = fix32::from_raw(c.y);
            v.bus = static_cast<Bus>(c.bus);
            v.loop = (c.flags & CMD_FLAG_LOOP) != 0;
            v.ducking = (c.flags & CMD_FLAG_DUCK) != 0;
            v.priority = static_cast<uint8_t>(c.priority);
            v.start_offset = offset;
            break;
        }
        case CmdType::Stop:
            if (Voice* v = pool_.find(c.voice_id)) v->active = false;
            break;
        case CmdType::SetBusGain:
            if (c.bus < BUS_N) bus_gain_[c.bus] = fix32::from_raw(c.gain);
            break;
        case CmdType::SetListener:
            listener_x_ = fix32::from_raw(c.x); listener_y_ = fix32::from_raw(c.y);
            break;
    }
}

void Mixer::drain_commands(uint32_t frames) {
    AudioCommand c;
    while (commands_.peek(c) && c.sample_time < cursor_ + frames) {
        commands_.pop(c);
        uint32_t off = c.sample_time > cursor_ ? static_cast<uint32_t>(c.sample_time - cursor_) : 0;
        apply(c, off);
    }
}

void Mixer::recompute_gains(uint32_t /*frames*/) {
    // duck считается ПО СЕМПЛУ в mix-петле (по факту «звучит ли ducking-голос», f>=start_offset),
    // не здесь — иначе target флипался бы на границе блока, а не на реальном старте звука
    // (block-зависимость). Тут — только per-voice gl/gr (константы в пределах блока).
    uint32_t active = 0;
    for (uint32_t i = 0; i < pool_.size(); ++i) {
        Voice& v = pool_[i];
        if (!v.active) continue;
        ++active;
        fix32 pan = clamp_pan((v.x - listener_x_) / PAN_DIST);
        fix32 panL = fix_sqrt(fix_clamp01((ONE - pan) * HALF));
        fix32 panR = fix_sqrt(fix_clamp01((ONE + pan) * HALF));
        fix32 dx = v.x - listener_x_, dy = v.y - listener_y_;
        fix32 dist = fix_sqrt(dx * dx + dy * dy);
        fix32 atten = fix_clamp01(ONE - dist / MAX_DIST);
        fix32 g = v.gain * atten;
        v.gl = g * panL;
        v.gr = g * panR;
    }
    if (active > peak_voices_) peak_voices_ = active;
}

void Mixer::mix(uint32_t frames, int16_t* out) {
    drain_commands(frames);
    recompute_gains(frames);
    if (backend_ == Backend::Fix32) mix_fix(frames, out);
    else mix_float(frames, out);
    // start_offset — intra-block старт: применяется ОДИН раз (в блоке рождения голоса), затем
    // гасится, иначе каждый следующий блок пропускал бы первые off фреймов (critical-фикс).
    for (uint32_t i = 0; i < pool_.size(); ++i) {
        Voice& v = pool_[i];
        if (v.active && v.start_offset)
            v.start_offset = v.start_offset > frames ? v.start_offset - frames : 0;
    }
    cursor_ += frames;
}

void Mixer::mix_fix(uint32_t frames, int16_t* out) {
    for (uint32_t f = 0; f < frames; ++f) {
        int64_t busL[BUS_N] = {0}, busR[BUS_N] = {0};
        bool duck_now = false;
        for (uint32_t i = 0; i < pool_.size(); ++i) {
            Voice& v = pool_[i];
            if (!v.active || f < v.start_offset) continue; // ещё не звучит в этом сэмпле
            if (v.ducking) duck_now = true;
            int64_t s = fetch_advance(v, underruns_);
            busL[static_cast<uint32_t>(v.bus)] += s * v.gl.raw;
            busR[static_cast<uint32_t>(v.bus)] += s * v.gr.raw;
        }
        duck_env_ = approach(duck_env_, duck_now ? DUCK_GAIN : ONE, DUCK_STEP); // per-sample рамп
        int64_t mL = 0, mR = 0;
        for (uint32_t b = 0; b < BUS_N; ++b) {
            fix32 bg = bus_gain_[b];
            if (b == static_cast<uint32_t>(Bus::Music)) bg = bg * duck_env_;
            mL += (busL[b] * bg.raw) >> 16;
            mR += (busR[b] * bg.raw) >> 16;
        }
        out[f * 2] = clamp16((mL * master_.raw) >> 32);
        out[f * 2 + 1] = clamp16((mR * master_.raw) >> 32);
    }
}

void Mixer::mix_float(uint32_t frames, int16_t* out) {
    for (uint32_t f = 0; f < frames; ++f) {
        double busL[BUS_N] = {0}, busR[BUS_N] = {0};
        bool duck_now = false;
        for (uint32_t i = 0; i < pool_.size(); ++i) {
            Voice& v = pool_[i];
            if (!v.active || f < v.start_offset) continue;
            if (v.ducking) duck_now = true;
            double s = fetch_advance(v, underruns_);
            busL[static_cast<uint32_t>(v.bus)] += s * v.gl.to_double();
            busR[static_cast<uint32_t>(v.bus)] += s * v.gr.to_double();
        }
        duck_env_ = approach(duck_env_, duck_now ? DUCK_GAIN : ONE, DUCK_STEP); // per-sample рамп
        double mL = 0, mR = 0;
        for (uint32_t b = 0; b < BUS_N; ++b) {
            double bg = bus_gain_[b].to_double();
            if (b == static_cast<uint32_t>(Bus::Music)) bg *= duck_env_.to_double();
            mL += busL[b] * bg;
            mR += busR[b] * bg;
        }
        out[f * 2] = clamp16(static_cast<int64_t>(std::lround(mL * master_.to_double())));
        out[f * 2 + 1] = clamp16(static_cast<int64_t>(std::lround(mR * master_.to_double())));
    }
}

} // namespace audio
