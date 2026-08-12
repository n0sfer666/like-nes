#include "framework_alloc_probe_control.hpp"

#include <cstdint>
#include <vector>

namespace framework::probe::control {
namespace {

// Над-выравненный тип: 64 байта — граница строки кэша, то есть значение, которое встречается в
// чужом коде, а не выдумано ради теста.
struct alignas(64) Wide {
    char pad[64];
};

// Указатель УЕЗЖАЕТ наружу и читается обратно, и обе половины несущие. Отдельного TU мало: тело
// функции компилятор видит целиком и сворачивает проверку в `true`, после чего выделение остаётся
// ненаблюдаемым и выбрасывается вместе с парным освобождением. Замер на воспроизводящем случае,
// счётчик обычных/выравненных выделений (числа из `plain`/`aligned`):
//
//                     наивный TU        с этим побегом
//   g++-16   -O2      2 / 0             2 / 1
//   g++-16   -O3      0 / 0             2 / 1
//   clang    -O2      2 / 0             2 / 1
//   clang    -O3      2 / 0             2 / 1
//
// То есть выравненный контроль наивный вынос ронял на ОБОИХ компиляторах — включая тот, на котором
// он до сих пор проходил. Запись в `volatile` сама по себе элизию не отменяла (проверено в раунде
// #15); отменяет её пара «запись — чтение обратно»: значение прочитанного компилятору неизвестно,
// поэтому разыменование обязано пойти в настоящую память, а блок под ним — существовать.
void* volatile g_escape = nullptr;

} // namespace

bool plain_allocation() {
    g_escape = new std::vector<int>();
    auto* v = static_cast<std::vector<int>*>(g_escape);
    v->resize(64);
    (*v)[63] = 1;
    const bool wrote = (*v)[63] == 1 && v->size() == 64;
    delete v;
    g_escape = nullptr;
    return wrote;
}

bool aligned_allocation() {
    g_escape = new Wide();
    auto* w = static_cast<Wide*>(g_escape);
    w->pad[0] = 1;
    const bool aligned = reinterpret_cast<std::uintptr_t>(w) % alignof(Wide) == 0 && w->pad[0] == 1;
    delete w;
    g_escape = nullptr;
    return aligned;
}

} // namespace framework::probe::control
