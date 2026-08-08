#pragma once
#include <cstdlib>
#include <new>

#include "platform_noinline.hpp"

// Счётчик обращений к куче для гейтов «горячий путь не аллоцирует»: гейт 7 спеки #14 (тик слоя) и
// гейт 6 спеки #15 (шаг физики).
//
// ТОЛЬКО ДЛЯ ТЕСТОВЫХ TU и ровно ОДНОГО на программу. Заголовок ОПРЕДЕЛЯЕТ глобальные
// operator new/delete — включённый в код, который куда-то линкуется, он подменит аллокатор всей
// программе; включённый дважды, даст переопределение. Функции не помечены `inline` намеренно:
// стандарт этого замещающим определениям не разрешает.
//
// Общий, потому что копия в каждом тесте — это не «немного дублирования». Набор перехваченных форм
// здесь неполон (выравненные перегрузки придут с формами вертикали 2), и разъехавшиеся копии дадут
// два гейта, считающих РАЗНОЕ под одним и тем же названием «ноль аллокаций».
namespace framework::probe {

inline bool in_hot = false;
inline long allocs = 0;

} // namespace framework::probe

// Запрет инлайна обязателен, а не косметика — почему, см. platform_noinline.hpp.

PLATFORM_NOINLINE void* operator new(std::size_t n) {
    if (framework::probe::in_hot) ++framework::probe::allocs;
    void* p = std::malloc(n ? n : 1);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}
PLATFORM_NOINLINE void* operator new[](std::size_t n) { return operator new(n); }
PLATFORM_NOINLINE void operator delete(void* p) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete[](void* p) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete(void* p, std::size_t) noexcept { std::free(p); }
PLATFORM_NOINLINE void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
