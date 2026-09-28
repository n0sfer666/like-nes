#pragma once
// Мутатор гейта 9: детерминированный, свой, без внешних зависимостей.
//
// Своим он сделан не из гордости, а по требованию: гейт обязан идти на ТРЁХ ОС и повторяться
// командой. libFuzzer отпадает по обеим осям разом — его нет на MSVC, и его прогон ограничен
// временем, то есть на быстром раннере он смотрит дальше, чем на медленном, и красный шаг не
// воспроизводится вовсе. Здесь вход в случай задаётся тройкой (цель, seed, номер), и одна и та же
// тройка даёт один и тот же буфер на любой машине: вся арифметика — беззнаковый 64-битный
// splitmix64, у которого нет ни переполнения со знаком, ни зависимости от размера `int`.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fuzz {

// splitmix64: одна строка состояния, нет таблиц, одинаков на всех трёх ОС. `rand()` сюда не
// годится — его последовательность часть libc, а не наша, и сравнивать прогоны раннеров было бы
// не с чем.
class Rng {
public:
    explicit Rng(uint64_t seed) : state_(seed) {}

    uint64_t next() {
        state_ += 0x9e3779b97f4a7c15ull;
        uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

    // Остаток, а не маска: длины буферов не степени двойки, и маска молча сузила бы выбор
    // смещения до нижней части файла — мутации не доезжали бы до хвоста таблицы.
    uint64_t below(uint64_t n) { return n == 0 ? 0 : next() % n; }

private:
    uint64_t state_;
};

// Значения, на которых ломаются счётчики и смещения: нули, максимумы каждой ширины и соседи
// максимума. Случайный байт попадает в такое число с вероятностью, близкой к нулю, а именно эти
// числа и вскрывали находки аудита — `n_after` в кадре плагина, `m_total_blocks` у KTX2.
inline uint64_t interesting(Rng& rng) {
    static const uint64_t table[] = {
        0ull,          1ull,          0x7full,       0x80ull,       0xffull,
        0x7fffull,     0x8000ull,     0xffffull,     0x7fffffffull, 0x80000000ull,
        0xfffffffeull, 0xffffffffull, 0x100000000ull, 0x7fffffffffffffffull,
        0x8000000000000000ull,        0xffffffffffffffffull,
    };
    return table[rng.below(sizeof(table) / sizeof(table[0]))];
}

// Один случай — это НЕСКОЛЬКО правок подряд, а не одна: одиночная правка почти всегда отбивается
// первой же проверкой конверта (магия, версия, размер), и до таблицы смещений прогон не доходит
// никогда. Число правок случайно от одной до восьми — мелкие остаются, чтобы проверять ранние
// сторожа, крупные пробивают их и уводят прогон вглубь.
inline std::vector<uint8_t> mutate(const std::vector<uint8_t>& seed, uint64_t key) {
    Rng rng(key);
    std::vector<uint8_t> buf = seed;
    const int edits = 1 + static_cast<int>(rng.below(8));
    for (int e = 0; e < edits; ++e) {
        // Пустой буфер — законный вход (усечение до нуля), и ни одна правка ниже на нём не
        // определена: выходим, а не подставляем байт, которого в буфере нет.
        if (buf.empty()) break;
        switch (rng.below(6)) {
            case 0: {  // бит: самая мелкая правка, проверяет сторожа на сходимость хеша
                const size_t at = static_cast<size_t>(rng.below(buf.size()));
                buf[at] = static_cast<uint8_t>(buf[at] ^ (1u << rng.below(8)));
                break;
            }
            case 1: {  // байт целиком
                buf[static_cast<size_t>(rng.below(buf.size()))] = static_cast<uint8_t>(rng.next());
                break;
            }
            case 2: {  // «интересное» число по выровненному смещению — счётчики и смещения живут
                       // в заголовке, и попасть в них случайным байтом нечем
                const uint64_t v = interesting(rng);
                const size_t width = static_cast<size_t>(1) << rng.below(4);  // 1,2,4,8
                if (buf.size() < width) break;
                const size_t at = static_cast<size_t>(rng.below(buf.size() - width + 1));
                // Младшие `width` байт — СДВИГАМИ, а не первыми байтами `uint64_t` через memcpy.
                // Первые байты совпадают с младшими только на little-endian: на big-endian все
                // «интересные» числа шириной 1/2/4 схлопнулись бы в 0x00 и 0xff, и обещание файла
                // («одна и та же тройка даёт один и тот же буфер на любой машине») перестало бы
                // держаться ровно там, где гейт и судит порядок байт. На LE байты те же, поэтому
                // зафиксированные числа прогона не сдвигаются.
                for (size_t b = 0; b < width; ++b) {
                    buf[at + b] = static_cast<uint8_t>(v >> (8 * b));
                }
                break;
            }
            case 3: {  // усечение: недокачанный файл, самый частый битый вход в жизни
                buf.resize(static_cast<size_t>(rng.below(buf.size())));
                break;
            }
            case 4: {  // удлинение мусором: заголовок обещает меньше, чем пришло
                const size_t add = static_cast<size_t>(1 + rng.below(64));
                for (size_t i = 0; i < add; ++i) buf.push_back(static_cast<uint8_t>(rng.next()));
                break;
            }
            default: {  // перестановка куска: раскладка ломается без единого нового байта
                const size_t n = static_cast<size_t>(1 + rng.below(buf.size()));
                const size_t from = static_cast<size_t>(rng.below(buf.size() - n + 1));
                const size_t to = static_cast<size_t>(rng.below(buf.size() - n + 1));
                std::vector<uint8_t> chunk(buf.begin() + static_cast<std::ptrdiff_t>(from),
                                           buf.begin() + static_cast<std::ptrdiff_t>(from + n));
                std::memcpy(buf.data() + to, chunk.data(), n);
                break;
            }
        }
    }
    return buf;
}

} // namespace fuzz
