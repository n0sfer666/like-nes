#include "mixer.hpp"

namespace audio {
namespace {

const fix32 HALF = fix32::from_float(0.5);
const fix32 ONE = fix32::from_int(1);
const fix32 PAN_DIST = fix32::from_int(8);       // полу-ширина панорамы (мир→[-1,1])
const fix32 MAX_DIST = fix32::from_int(24);      // радиус слышимости (linear rolloff)
constexpr uint32_t BUS_N = static_cast<uint32_t>(Bus::Count);

fix32 clamp_pan(fix32 p) {
    if (p.raw > ONE.raw) return ONE;
    if (p.raw < -ONE.raw) return fix32::from_raw(-ONE.raw);
    return p;
}


} // namespace

Mixer::Mixer(Backend b) : backend_(b), master_(fix32::from_int(1)), duck_env_(fix32::from_int(1)) {
    for (uint32_t i = 0; i < BUS_N; ++i) bus_gain_[i] = fix32::from_int(1);
}

void Mixer::register_source(uint64_t guid, const int16_t* pcm, uint32_t frames, SampleRing* ring) {
    if (source_count_ >= MAX_SOURCES) return;
    Source source; source.guid = guid; source.pcm = pcm; source.frames = frames; source.ring = ring;
    sources_[source_count_++] = source;
}

bool Mixer::register_stereo_source(uint64_t guid, const StereoFrame* pcm, uint32_t frames) {
    if (source_count_ == MAX_SOURCES || find_source(guid) || (!pcm && frames)) return false;
    Source source; source.guid = guid; source.frames = frames; source.stereo_pcm = pcm; source.stereo = true;
    sources_[source_count_++] = source;
    return true;
}

const Source* Mixer::find_source(uint64_t guid) const {
    for (uint32_t i = 0; i < source_count_; ++i)
        if (sources_[i].guid == guid) return &sources_[i];
    return nullptr;
}

// Громкость из команды — доля в [0,1] (аудит #21 A·3·4): выше единицы — усиление за полную шкалу и
// срез, ниже нуля — инверсия фазы вместо тишины. Кламп здесь, на шве консюмера, а не в AudioEngine:
// команду в очередь кладёт и тот, кто собрал её руками.
void Mixer::apply(const AudioCommand& c, uint32_t offset) {
    switch (static_cast<CmdType>(c.type)) {
        case CmdType::Play: {
            if (c.bus >= BUS_N) return; // невалидная шина → drop (иначе OOB busL[]/busR[])
            const Source* src = find_source(c.guid);
            if (!src) return; // отсутствует ассет → тишина-placeholder, не crash
            Voice& v = pool_.alloc(seq_++);
            v.id = c.voice_id;
            v.pcm = src->pcm; v.frames = src->frames; v.ring = src->ring;
            v.stereo_pcm = src->stereo_pcm; v.stereo = src->stereo;
            v.gain = fix_clamp01(fix32::from_raw(c.gain));
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
            if (c.bus < BUS_N) bus_gain_[c.bus] = fix_clamp01(fix32::from_raw(c.gain));
            break;
        case CmdType::SetListener:
            listener_x_ = fix32::from_raw(c.x); listener_y_ = fix32::from_raw(c.y);
            break;
        case CmdType::SetMasterGain:
            master_ = fix_clamp01(fix32::from_raw(c.gain));
            break;
    }
}

void Mixer::drain_commands(uint32_t frames) {
    AudioCommand c;
    const uint64_t cur = cursor();
    while (commands_.peek(c) && c.sample_time < cur + frames) {
        commands_.pop(c);
        uint32_t off = c.sample_time > cur ? static_cast<uint32_t>(c.sample_time - cur) : 0;
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
        if (v.stereo) { v.gl = v.gr = v.gain; continue; }
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
    const uint32_t peak = peak_voices_.load(std::memory_order_relaxed);
    if (active > peak) peak_voices_.store(active, std::memory_order_relaxed);
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
    cursor_.store(cursor() + frames, std::memory_order_relaxed);
}

} // namespace audio
