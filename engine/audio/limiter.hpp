#pragma once
#include <cstdint>

#include "audio_types.hpp"

// Ограничитель выхода (аудит #21 A·3·1). Коррелированные голоса складываются почти арифметически, и
// жёсткий клип превращал их сумму в прямоугольник полной шкалы. Здесь громкость снижается целиком,
// а форма волны сохраняется. Выход задержан на LOOKAHEAD - 1 кадров (~1 мс): нужное семплу
// усиление берётся минимумом по окну упреждения и сглаживается средним по окну той же длины —
// каждое слагаемое среднего видело выходящий семпл, поэтому ни один семпл не выходит выше порога,
// а огибающая сходит к пику без ступеньки. Удержание — минимум этого усиления за последние 20 мс:
// пики низкого тона, идущие чаще, держат его ровным, а не перезапускают релиз между собой. Затем
// экспоненциальный релиз ~50 мс. Целые числа и один код на оба бэкенда — Fix32 и Float
// ограничиваются одинаково. Пока сумма ниже порога, огибающая равна единице и семплы проходят без
// изменений.
namespace audio {

class Limiter {
public:
    static constexpr int64_t THRESHOLD = 29204; // −1 dBFS
    static constexpr int64_t UNITY = 1 << 16;
    static constexpr uint32_t LOOKAHEAD = SAMPLE_RATE / 1000;
    static constexpr uint32_t HOLD = SAMPLE_RATE / 50;
    static constexpr int64_t RELEASE = 27; // доля 1/2400 за семпл в Q16: ~50 мс на 48 кГц

    Limiter() {
        for (int64_t& n : need_) n = UNITY;
        for (int64_t& m : min_) m = UNITY;
    }

    // Кладёт кадр в задержку и пишет в out кадр, вошедший LOOKAHEAD - 1 вызовов назад.
    void apply(int64_t l, int64_t r, int16_t* out) {
        left_[pos_] = l;
        right_[pos_] = r;
        need_[pos_] = need(l, r);
        pos_ = (pos_ + 1) % LOOKAHEAD;
        int64_t m = UNITY;
        for (int64_t n : need_) m = n < m ? n : m;
        sum_ += m - min_[pos_];
        min_[pos_] = m;
        const int64_t held = hold(sum_ / LOOKAHEAD);
        if (held < env_) {
            env_ = held;
        } else {
            const int64_t up = env_ + (((UNITY - env_) * RELEASE + UNITY - 1) >> 16);
            env_ = up < held ? up : held;
        }
        out[0] = static_cast<int16_t>((left_[pos_] * env_) >> 16);
        out[1] = static_cast<int16_t>((right_[pos_] * env_) >> 16);
    }

private:
    static int64_t mag(int64_t v) { return v < 0 ? -v : v; }
    static int64_t need(int64_t l, int64_t r) {
        const int64_t peak = mag(l) > mag(r) ? mag(l) : mag(r);
        return peak > THRESHOLD ? (THRESHOLD << 16) / peak : UNITY;
    }

    // Минимум за последние HOLD кадров: монотонная очередь в кольце на HOLD мест. Каждое значение
    // входит и выходит один раз, так что в среднем — O(1) на кадр, без аллокаций.
    int64_t hold(int64_t target) {
        if (count_ && frame_ - held_at_[head_] >= HOLD) {
            head_ = (head_ + 1) % HOLD;
            --count_;
        }
        while (count_ && held_[(head_ + count_ - 1) % HOLD] >= target) --count_;
        const uint32_t tail = (head_ + count_) % HOLD;
        held_[tail] = target;
        held_at_[tail] = frame_++;
        ++count_;
        return held_[head_];
    }

    int64_t left_[LOOKAHEAD] = {}, right_[LOOKAHEAD] = {};
    int64_t need_[LOOKAHEAD], min_[LOOKAHEAD];
    int64_t sum_ = UNITY * LOOKAHEAD;
    uint32_t pos_ = 0;
    int64_t held_[HOLD];
    uint32_t held_at_[HOLD];
    uint32_t head_ = 0, count_ = 0, frame_ = 0;
    int64_t env_ = UNITY;
};

} // namespace audio
