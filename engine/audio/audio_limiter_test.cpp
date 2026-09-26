#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "engine.hpp"
#include "limiter.hpp"
#include "mixer.hpp"

// Аудит #21 A·3·1: восемь одинаковых голосов разом — сумма далеко за полной шкалой. Жёсткий клип
// отдавал на выход серии ±32767 — прямоугольник, скачок громкости с резкой атакой. Ограничитель
// обязан снизить громкость, сохранив форму волны, и отпустить её после залпа. Потолок −1 dBFS
// считается здесь, а не берётся из Limiter: иначе тест сравнивал бы константу саму с собой.
using namespace audio;

namespace {

constexpr uint32_t SRC = 4800, BLOCK = 480, TOTAL = SAMPLE_RATE + 9600, LATE = SAMPLE_RATE;
constexpr uint32_t DELAY = SAMPLE_RATE / 1000 - 1, MS = SAMPLE_RATE / 1000;
const int CEIL = static_cast<int>(std::lround(32767 * std::pow(10.0, -1.0 / 20)));
int fails = 0;

std::vector<int16_t> sine() {
    std::vector<int16_t> s(SRC);
    for (uint32_t i = 0; i < SRC; ++i)
        s[i] = static_cast<int16_t>(29490.0 * std::sin(6.283185307179586 * 440.0 * i / SAMPLE_RATE));
    return s;
}

std::vector<int16_t> render(Backend b, const std::vector<int16_t>& src, int burst, uint32_t block = BLOCK) {
    Mixer mix(b);
    mix.register_source(1, src.data(), SRC, nullptr);
    AudioEngine eng(mix);
    PlayParams p;
    for (int k = 0; k < burst; ++k) eng.play(1, p, 0);
    p.gain = fix32::from_float(0.5);
    eng.play(1, p, LATE);
    std::vector<int16_t> out(TOTAL * OUT_CHANNELS);
    for (uint32_t f = 0; f < TOTAL; f += block) mix.mix(block, out.data() + f * OUT_CHANNELS);
    return out;
}

void check(bool ok, const char* what, const char* where) {
    if (!ok) {
        std::printf("[audio-limiter] FAIL: %s (%s)\n", what, where);
        ++fails;
    }
}

void check(bool ok, Backend b, const char* what) { check(ok, what, b == Backend::Fix32 ? "fix32" : "float"); }

std::vector<int> limit(const std::vector<int64_t>& in) {
    Limiter lim;
    std::vector<int> out(in.size());
    int16_t fr[2];
    for (size_t i = 0; i < in.size(); ++i) {
        lim.apply(in[i], in[i], fr);
        out[i] = fr[0];
    }
    return out;
}

// Ниже порога семпл проходит бит-в-бит, только задержанный: у Float-бэкенда голдена нет.
void passthrough() {
    std::vector<int64_t> in(SRC);
    uint32_t x = 1;
    for (int64_t& v : in) v = static_cast<int64_t>((x = x * 1664525u + 1013904223u) % (2u * CEIL + 1)) - CEIL;
    in[100] = CEIL;
    in[200] = -CEIL;
    const std::vector<int> out = limit(in);
    bool same = true;
    for (uint32_t i = 0; i < SRC; ++i) same = same && out[i] == (i < DELAY ? 0 : in[i - DELAY]);
    check(same, "a signal up to the ceiling passes bit-exact, delayed", "limiter");
    const std::vector<int> over = limit(std::vector<int64_t>(SRC, CEIL + 1));
    check(over[SRC - 1] < CEIL + 1, "one step above the ceiling is turned down", "limiter");
}

// Залп 10 мс вдвое выше порога, затем ровный тон ниже: через 18 мс усиление ещё держится, через
// 30 мс уже плавно отпускается — не ступенькой, через 150 мс вернулось в пределы 1 dB.
void hold_and_release() {
    std::vector<int64_t> in(200 * MS, 10000);
    for (uint32_t i = 0; i < 10 * MS; ++i) in[i] = 2 * CEIL;
    const std::vector<int> out = limit(in);
    check(out[28 * MS + DELAY] < 5100, "the gain is held 18 ms after a burst", "limiter");
    check(out[40 * MS + DELAY] > 5200 && out[40 * MS + DELAY] < 6500, "the release starts 20 ms after a burst", "limiter");
    check(out[60 * MS + DELAY] > 6900 && out[60 * MS + DELAY] < 7700, "the release is gradual, not a step", "limiter");
    check(out[160 * MS + DELAY] >= 8913, "the gain is back within 1 dB 150 ms after a burst", "limiter");
}

// Пики низкого тона выше порога идут чаще удержания: усиление между ними стоит ровно, а не
// отпускается и падает снова на каждом пике.
void low_tone(double hz, const char* where) {
    std::vector<int64_t> in(SAMPLE_RATE);
    for (uint32_t i = 0; i < SAMPLE_RATE; ++i) in[i] = std::llround(3.0 * CEIL * std::sin(6.283185307179586 * hz * i / SAMPLE_RATE));
    const std::vector<int> out = limit(in);
    double lo = 1, hi = 0;
    for (uint32_t i = 200 * MS; i + DELAY < SAMPLE_RATE; ++i) {
        if (std::llabs(in[i]) < CEIL) continue;
        const double g = static_cast<double>(out[i + DELAY]) / static_cast<double>(in[i]);
        lo = g < lo ? g : lo;
        hi = g > hi ? g : hi;
    }
    check(hi > 0 && (hi - lo) / hi < 0.01, "a steady low tone above the ceiling is not pumped", where);
}

} // namespace

int main() {
    const std::vector<int16_t> src = sine();
    for (Backend b : {Backend::Fix32, Backend::Float}) {
        const std::vector<int16_t> loud = render(b, src, 8), lone = render(b, src, 0);
        int rails = 0, flat = 0, run = 1, peak = 0;
        for (uint32_t i = 0; i < SRC * OUT_CHANNELS; ++i) {
            const int v = loud[i], a = v < 0 ? -v : v;
            if (v >= 32767 || v <= -32767) ++rails;
            if (a > peak) peak = a;
            run = (i >= OUT_CHANNELS && v == loud[i - OUT_CHANNELS] && a > CEIL / 2) ? run + 1 : 1;
            if (run == 3) ++flat;
        }
        check(rails == 0, b, "correlated voices above full scale reach the rail");
        check(flat == 0, b, "correlated voices above full scale are flattened, not turned down");
        check(peak <= CEIL, b, "the limited burst stays under -1 dBFS");
        check(peak > CEIL * 9 / 10, b, "the limited burst is still loud");
        bool same = true;
        for (uint32_t i = LATE * OUT_CHANNELS; i < TOTAL * OUT_CHANNELS; ++i) same = same && loud[i] == lone[i];
        check(same, b, "the limiter lets go a second after the burst");
        check(render(b, src, 8, 1) == loud, b, "the limited burst does not depend on the block size");
    }
    passthrough();
    hold_and_release();
    low_tone(60, "60 Hz");
    low_tone(100, "100 Hz");
    if (fails) {
        std::printf("audio-limiter: FAIL (%d)\n", fails);
        return 1;
    }
    std::printf("audio-limiter: PASS - 20 cases\n");
    return 0;
}
