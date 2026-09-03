#pragma once

#include <chrono>
#include <cstdint>

// Искусственная глухота пира — оснастка гейта 7 (спека #22): на заданном тике пир перестаёт читать
// сокет и не читает его заданное число миллисекунд.
//
// Своим файлом, потому что это ЕДИНСТВЕННОЕ в проходе, чего в настоящей игре не бывает вовсе:
// сбой, который гейт вносит сам. Остальные шаги прохода — очередь ввода, догон, отказ ждать дыру —
// работа пира при любом прогоне, и держать среди них ещё и симулятор сбоя значило бы читать
// каждый проход через «а это по-настоящему или гейтом подстроено».
//
// Окно меряется СТЕННЫМ временем, а границы отчитываются в ТИКАХ. Первое — потому что спорит оно с
// величинами стенного времени (терпением к дыре и дедлайном), второе — потому что гейт обязан
// доказать, что молчание случилось посередине прогона: окно, открывшееся на финишной черте, не
// стоило бы ничего, а «сошлись по хешу» было бы сказано про обычный прогон.
namespace platformer {

class Deafness {
  public:
    void arm(uint32_t at, uint32_t ms) {
        at_ = at;
        ms_ = ms;
        armed_ = ms > 0;
    }

    // Взводится и гаснет РОВНО ОДИН РАЗ: `armed_` не возвращается, иначе пир на длинном прогоне
    // глох бы снова и снова, а отчёт называл бы границы последнего окна как единственного.
    void update(uint32_t tick) {
        if (armed_ && !running_) {
            if (tick < at_) return;
            running_ = true;
            since_ = Clock::now();
            from_ = tick;
        }
        if (running_ && elapsed_ms() >= static_cast<int64_t>(ms_)) {
            running_ = false;
            armed_ = false;
            to_ = tick;
        }
    }

    bool running() const { return running_; }

    // Проход внутри окна, на котором пир НЕ СМОГ шагнуть. Разность границ этого не говорит:
    // получатель отстаёт по тикам от собственного известного ввода и за время молчания честно
    // доигрывает накопленный запас — сколько именно, задаёт загруженность машины, а не глухота.
    void stalled() {
        if (running_) ++stalls_;
    }

    uint32_t from() const { return from_; }
    uint32_t to() const { return to_; }
    uint32_t stalls() const { return stalls_; }

  private:
    using Clock = std::chrono::steady_clock;

    int64_t elapsed_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - since_).count();
    }

    Clock::time_point since_{};
    uint32_t at_ = 0;
    uint32_t ms_ = 0;
    uint32_t from_ = 0;
    uint32_t to_ = 0;
    uint32_t stalls_ = 0;
    bool armed_ = false;
    bool running_ = false;
};

} // namespace platformer
