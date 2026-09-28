// Сборка таблицы целей и ПОЗИТИВНЫЙ КОНТРОЛЬ гейта 9.
//
// Контроль — не украшение: без него зелёный прогон значит «ни одна цель не упала», а это ровно то
// же самое, что «мутатор выдаёт мусор, который отвергается на первом байте», «цель ничего не
// читает после `open()`» и «санитайзер не включён». Три разных отказа с одним видом. Канарейка —
// читатель, у которого проверка длины СНЯТА нарочно, и шаг CI красен, если она НЕ упала.
#include <cstring>
#include <vector>

#include "fuzz_target.hpp"

namespace fuzz {
namespace {

// Раскладка канарейки: `LNCN`, число записей, записи. Число — единственное поле, и снятая проверка
// ровно одна, чтобы падение нельзя было списать на второй дефект.
const uint8_t CANARY_MAGIC[4] = {'L', 'N', 'C', 'N'};
constexpr uint32_t CANARY_ROWS = 4;

std::vector<uint8_t> seed_canary() {
    std::vector<uint8_t> out(8 + CANARY_ROWS * 4, 0);
    std::memcpy(out.data(), CANARY_MAGIC, 4);
    const uint32_t rows = CANARY_ROWS;
    std::memcpy(out.data() + 4, &rows, 4);
    for (uint32_t i = 0; i < CANARY_ROWS; ++i) {
        const uint32_t v = 0xa5a50000u | i;
        std::memcpy(out.data() + 8 + i * 4, &v, 4);
    }
    return out;
}

bool read_canary(const uint8_t* data, size_t size) {
    if (size < 8) return false;
    if (std::memcmp(data, CANARY_MAGIC, 4) != 0) return false;
    uint32_t rows = 0;
    std::memcpy(&rows, data + 4, 4);
    // СНЯТО НАРОЧНО: `if (8 + static_cast<uint64_t>(rows) * 4 > size) return false;`
    // Это и есть предмет контроля — заголовок, которому поверили на слово.
    for (uint32_t i = 0; i < rows; ++i) {
        uint32_t v = 0;
        std::memcpy(&v, data + 8 + static_cast<size_t>(i) * 4, 4);
        consume(v);
    }
    return true;
}

const Target CANARY = {"canary", seed_canary, read_canary};

void append(std::vector<Target>& all, const Target* (*group)(std::size_t*)) {
    std::size_t n = 0;
    const Target* t = group(&n);
    for (std::size_t i = 0; i < n; ++i) all.push_back(t[i]);
}

const std::vector<Target>& table() {
    static const std::vector<Target> all = [] {
        std::vector<Target> v;
        append(v, engine_targets);
        append(v, ach_targets);
        append(v, framework_targets);
        append(v, plugin_targets);
#if defined(LN_FUZZ_SCENE)
        append(v, scene_targets);
#endif
        return v;
    }();
    return all;
}

} // namespace

const Target* targets(std::size_t* count) {
    const std::vector<Target>& all = table();
    *count = all.size();
    return all.data();
}

const Target& canary() { return CANARY; }

} // namespace fuzz
