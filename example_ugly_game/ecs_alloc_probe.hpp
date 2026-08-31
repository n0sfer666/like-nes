#pragma once
#include <flecs.h>

#include "framework_alloc_probe.hpp"

// ВТОРОЙ счётчик кадра, и без него ECS-половина гейта 8 была бы вакуумной. `framework::probe`
// подменяет глобальные `operator new`/`delete` — а flecs написан на C и ходит в кучу через
// `ecs_os_api.malloc_`, то есть мимо него ЦЕЛИКОМ: сборка запроса на каждом кадре внутри окна дала
// счётчику фреймворка честный ноль, и «ноль» там значил «не смотрю», а не «не аллоцирует».
// Поэтому ECS считается своим швом — тем самым, который flecs для этого и держит.
namespace game::ecs_probe {

inline long allocs = 0;

inline ecs_os_api_malloc_t base_malloc = nullptr;
inline ecs_os_api_calloc_t base_calloc = nullptr;
inline ecs_os_api_realloc_t base_realloc = nullptr;

inline void* count_malloc(ecs_size_t size) {
    if (framework::probe::in_hot) ++allocs;
    return base_malloc(size);
}

inline void* count_calloc(ecs_size_t size) {
    if (framework::probe::in_hot) ++allocs;
    return base_calloc(size);
}

// `realloc` считается наравне: переезд буфера — это та же ходка в кучу, и запрос, пересобираемый
// за кадр, отличался бы от установившегося именно ею.
inline void* count_realloc(void* ptr, ecs_size_t size) {
    if (framework::probe::in_hot) ++allocs;
    return base_realloc(ptr, size);
}

// Ставится ДО первого мира: `ecs_os_set_api` меняет таблицу, которую мир забирает при создании.
inline void install() {
    ecs_os_init();
    ecs_os_api_t api = ecs_os_get_api();
    base_malloc = api.malloc_;
    base_calloc = api.calloc_;
    base_realloc = api.realloc_;
    api.malloc_ = count_malloc;
    api.calloc_ = count_calloc;
    api.realloc_ = count_realloc;
    ecs_os_set_api(&api);
}

} // namespace game::ecs_probe
