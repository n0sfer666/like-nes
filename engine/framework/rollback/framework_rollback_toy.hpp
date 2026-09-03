#pragma once
#include <cstdint>

// Обстановка гейта сессии: симуляция ровно того вида, которого требует `Session`, и скрипт ввода
// к ней. Отдельно от утверждений по той же границе, что у гейтов платформера: здесь то, ЧТО гоняют,
// там то, что про это утверждают.
//
// Симуляция игрушечная НАМЕРЕННО. Гейт спрашивает про сессию — про кольца, предсказание и
// переигрывание, — а не про физику: сцена на fix32 отвечала бы расхождением хеша и на сломанном
// откате, и на сломанном снимке, и на сломанном решателе. Целочисленное состояние оставляет ровно
// одно объяснение. Настоящий откат настоящей симуляции проверяется гейтом игры-образца.
namespace framework::rollback::toy {

inline uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    h ^= h >> 31;
    h *= 0xd6e8feb86659fd93ull;
    return h ^ (h >> 32);
}

// Полей ровно два вида, и это не украшение: `pos` шаг МЕНЯЕТ, а `hash` он накапливает. Снимок,
// вернувший позиции и забывший накопленное, — самая правдоподобная поломка отката, и без второго
// поля она была бы неотличима от верной работы.
struct ToyState {
    int64_t pos[2] = {0, 0};
    uint64_t hash = 0;
    uint64_t steps = 0;
};

struct ToyInput {
    int32_t dx = 0;
    int32_t fire = 0;
};

struct ToySim {
    using Input = ToyInput;
    using Snapshot = ToyState;

    static constexpr uint32_t PLAYERS = 2;

    ToyState state;

    void save(Snapshot& out) const { out = state; }
    void restore(const Snapshot& in) { state = in; }

    void step(const Input* in) {
        for (uint32_t p = 0; p < PLAYERS; ++p) {
            state.pos[p] += in[p].dx;
            // Ветка, а не только сложение: ввод, влияющий на состояние НЕЛИНЕЙНО, не даёт откату
            // сойтись случайно. Сложение коммутативно, и прогон, переигравший тики не в том
            // порядке, дал бы ту же сумму.
            //
            // Остаток здесь не для красоты чисел: без него произведение за две сотни тиков уходит
            // за int64, а знаковое переполнение — UB, то есть фикстура гейта сама ломала бы
            // санитайзерный шаг (`UndefinedBehaviorSanitizer: signed integer overflow`, прогон
            // 2026-09-03). Диапазон обрезан там, где нелинейность уже состоялась.
            if (in[p].fire != 0) state.pos[p] = (state.pos[p] * 3 + 1) % 1000003;
        }
        ++state.steps;
        state.hash = mix(mix(mix(state.hash, static_cast<uint64_t>(state.pos[0])),
                             static_cast<uint64_t>(state.pos[1])),
                         state.steps);
    }
};

// Снимок, забывший накопленное. Позитивный контроль гейта: на нём совпадение хешей обязано
// пропасть. Без него гейт доказывает лишь то, что прогон детерминирован, — а он детерминирован и
// без всякого отката.
struct LossySim : ToySim {
    void restore(const Snapshot& in) {
        const uint64_t keep = state.hash;
        state = in;
        state.hash = keep;
    }
};

// Скрипт ввода — функция от тика и игрока, а не запись: прогон обязан быть воспроизводим обеими
// сторонами гейта, а второй буфер стал бы второй правдой о нём.
inline ToyInput scripted(uint32_t t, uint32_t p) {
    const uint64_t r = mix(t + 1, p + 1);
    ToyInput in;
    in.dx = static_cast<int32_t>(r % 7u) - 3;
    in.fire = ((r >> 8) % 5u == 0) ? 1 : 0;
    return in;
}

// Ввод, который предсказание угадывает всегда: одно и то же на каждом тике. Им проверяется, что
// верная догадка стоит ноль переигранных тиков, — реализация, откатывающаяся на каждый пришедший
// пакет, проходит гейт совпадения хешей и валит бюджет кадра.
inline ToyInput steady(uint32_t, uint32_t p) {
    ToyInput in;
    in.dx = 2 + static_cast<int32_t>(p);
    return in;
}

} // namespace framework::rollback::toy
