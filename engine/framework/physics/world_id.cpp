#include <cstdio>
#include <cstdlib>

#include "world.hpp"

namespace framework::physics {

// Проверка стоит во ВСЕХ сборках, а не под NDEBUG (аудит #21 B10): `INVALID` от `add` с занятым
// ключом — случай загрузки уровня из данных, то есть релизной игры, и в ней чтение по индексу
// 0xffffffff давало бы мусорное тело, а не падение. Сравнение с размером на ручку — цена, которой
// горячий путь шага не платит: шаг ходит по `bodies_` сам, без дескрипторов.
uint32_t World::checked(BodyId id) const {
    if (id.index < bodies_.size()) return id.index;
    // `INVALID` — не только отказ `add`, но и значение по умолчанию у неприсвоенного дескриптора
    // (`Stage::lift`, `Character::support`), поэтому строка называет оба источника.
    if (id.valid())
        std::fprintf(stderr, "[physics] body id %u is past the last body (%zu bodies)\n", id.index,
                     bodies_.size());
    else
        std::fprintf(stderr, "[physics] body id is INVALID (a refused add() or an id never assigned)\n");
    std::abort();
}

} // namespace framework::physics
