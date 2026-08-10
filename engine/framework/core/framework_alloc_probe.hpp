#pragma once
#include <cstdint>
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
// Общий, потому что копия в каждом тесте — это не «немного дублирования»: разъехавшиеся копии дали
// бы два гейта, считающих РАЗНОЕ под одним и тем же названием «ноль аллокаций».
//
// Перехвачены и ВЫРАВНЕННЫЕ формы. Пока их не было, дыра была тихой в обе стороны: аллокация
// над-выравненного типа шла мимо счётчика (гейт зелен, куча тронута), а её `delete` уходил в
// `std::free` от указателя, которого `malloc` не выдавал, — то есть гейт не просто недосчитывал, он
// портил кучу. Форма перегрузки выбирается КОМПИЛЯТОРОМ по `alignof` типа, а не автором кода,
// поэтому дыра открылась бы от одного `alignas(32)` в структуре, до которой шаг физики дотянулся.
namespace framework::probe {

inline bool in_hot = false;
inline long allocs = 0;

// Выравненное выделение сделано вручную поверх `malloc`, а не через `aligned_alloc`/`_aligned_malloc`:
// первого нет у MSVC, второй требует парного `_aligned_free`, и выбор между ними — условная
// компиляция по компилятору в файле, который к платформенному слою не относится. Ручная разметка
// платформо-независима целиком, а цена её — одно лишнее слово на выделение — здесь не важна: проба
// живёт только в тестовых программах.
inline void* aligned_raw(std::size_t n, std::size_t align) {
    if (align < alignof(void*)) align = alignof(void*);
    // Сумма проверяется ДО сложения, а не после: переполнение `std::size_t` — не «большое число», а
    // маленькое, и `malloc` на него честно вернёт крошечный блок. Запись разметки ушла бы за его
    // край, то есть проба, заведённая ловить порчу кучи, сама бы её и устроила. Слагаемых три,
    // поэтому и проверки две: `SIZE_MAX - align - sizeof(void*)` при достаточно большом `align`
    // переполняется САМ, и однострочная проверка пропускала бы ровно тот случай, ради которого
    // написана.
    if (align > SIZE_MAX - sizeof(void*)) return nullptr;
    if (n > SIZE_MAX - align - sizeof(void*)) return nullptr;
    void* raw = std::malloc(n + align + sizeof(void*));
    if (raw == nullptr) return nullptr;
    // Исходный указатель кладётся в слово ПЕРЕД выданным: `free` обязан получить ровно то, что
    // вернул `malloc`, а вычислить это по выданному адресу нечем — сдвиг зависит от того, насколько
    // не выровнен оказался сам `malloc`.
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(raw) + sizeof(void*);
    const std::uintptr_t aligned = (base + align - 1) & ~(static_cast<std::uintptr_t>(align) - 1);
    void* out = reinterpret_cast<void*>(aligned);
    static_cast<void**>(out)[-1] = raw;
    return out;
}

inline void aligned_free(void* p) {
    if (p == nullptr) return;
    std::free(static_cast<void**>(p)[-1]);
}

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

PLATFORM_NOINLINE void* operator new(std::size_t n, std::align_val_t a) {
    if (framework::probe::in_hot) ++framework::probe::allocs;
    void* p = framework::probe::aligned_raw(n ? n : 1, static_cast<std::size_t>(a));
    if (p == nullptr) throw std::bad_alloc();
    return p;
}
PLATFORM_NOINLINE void* operator new[](std::size_t n, std::align_val_t a) {
    return operator new(n, a);
}
PLATFORM_NOINLINE void operator delete(void* p, std::align_val_t) noexcept {
    framework::probe::aligned_free(p);
}
PLATFORM_NOINLINE void operator delete[](void* p, std::align_val_t) noexcept {
    framework::probe::aligned_free(p);
}
PLATFORM_NOINLINE void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    framework::probe::aligned_free(p);
}
PLATFORM_NOINLINE void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    framework::probe::aligned_free(p);
}
