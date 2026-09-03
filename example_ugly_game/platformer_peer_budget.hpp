#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>

// Чем меряется гейт 8 спеки #22: сколько стенного времени пир тратит за проход на симуляцию и
// сколько — на сокет.
//
// Цифры ДВЕ, а не одна. Вопрос гейта — влезает ли кадр в 16.67 мс, и на него отвечает уже сумма, но
// сумма не говорит, что именно кадр съело: переигрывание тиков при откате лежит внутри `advance`,
// то есть в цифре симуляции, а переотправка и разбор датаграмм — в цифре сети. Перерасход без этого
// разделения читался бы как «движок медленный» и на прогоне, где медленным был транспорт.
//
// Сон петли (1 мс, когда шагнуть нечем) не входит ни в одну: он не работа кадра, а способ не жечь
// процессор в ожидании соседа. На Windows он к тому же округляется вверх до ~15.6 мс и один
// перекрыл бы весь бюджет — цифра описывала бы гранулярность сна планировщика, а не движок.
namespace platformer::budget {

struct Meter {
    int64_t total_ns = 0;
    int64_t worst_ns = 0;
    uint32_t samples = 0;
};

// Наблюдение записывается ДЕСТРУКТОРОМ, поэтому измеряемое всегда целиком внутри области видимости
// пробы — включая выход из неё через `return` или `break`, которых в петле пира два.
class Span {
  public:
    using Clock = std::chrono::steady_clock;

    explicit Span(Meter& m) : m_(m), started_(Clock::now()) {}

    ~Span() {
        const int64_t ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started_).count();
        m_.total_ns += ns;
        if (ns > m_.worst_ns) m_.worst_ns = ns;
        ++m_.samples;
    }

    Span(const Span&) = delete;
    Span& operator=(const Span&) = delete;

  private:
    Meter& m_;
    Clock::time_point started_;
};

inline double worst_ms(const Meter& m) { return static_cast<double>(m.worst_ns) / 1e6; }

inline double mean_ms(const Meter& m) {
    return m.samples == 0 ? 0.0 : static_cast<double>(m.total_ns) / m.samples / 1e6;
}

// Проба, не заведённая вовсе, печатает те же нули, что и мгновенная работа: «кадр ничего не стоил»
// и «мерить забыли» с одного взгляда неразличимы, а значит гейт 8 был бы зелен на замере, которого
// нет. Отсюда утверждение, а не только печать.
//
// Наблюдений у симуляции ровно столько, сколько сыграно тиков: меньше — проба стоит не в том месте,
// больше — она внутри переигрывания отката, и цена одного тика посчитана несколько раз. У сокета
// такого числа нет вовсе, проходов цикла больше, чем тиков, и спросить с него можно лишь то, что
// он вообще наблюдался.
inline bool swept(const Meter& sim, const Meter& net, uint32_t ticks) {
    return sim.samples == ticks && sim.total_ns > 0 && net.samples > 0 && net.total_ns > 0;
}

inline void report(const char* role, const Meter& sim, const Meter& net) {
    std::printf("  peer %s: sim worst=%.3f ms mean=%.3f ms over %u ticks\n", role, worst_ms(sim),
                mean_ms(sim), sim.samples);
    std::printf("  peer %s: net worst=%.3f ms mean=%.3f ms over %u passes\n", role, worst_ms(net),
                mean_ms(net), net.samples);
}

} // namespace platformer::budget
