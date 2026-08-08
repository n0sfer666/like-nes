#pragma once
#include <vector>

#include "body.hpp"
#include "contact.hpp"

// Широкая фаза: из N тел отобрать пары, которые вообще могут касаться, не проверяя все N².
//
// Здесь sweep-and-prune по X, а не равномерная сетка, и это выбор ПО ОТСУТСТВИЮ КОНСТАНТЫ:
// сетке нужен размер ячейки, подобранный под типичный размер тела, а типичного размера у нас
// ещё нет — первая игра на этой физике появится в раунде #16. SAP не настраивается ничем.
// Замер на живой сцене — вертикаль 3 спеки #15; подмена реализации golden-хеш не сдвинет:
// порядок решения задаёт сортировка пар по стабильным ключам ниже, а не порядок обхода.
namespace framework::physics {

class Broadphase {
public:
    void reserve(uint32_t capacity);

    // `out` очищается и заполняется парами, отсортированными по (key_a, key_b). Вызывающий
    // отвечает за ёмкость `out`: внутри шага куча запрещена (гейт 6 спеки #15).
    void build(const std::vector<Body>& bodies, std::vector<Pair>& out);

private:
    std::vector<uint32_t> order_;
    std::vector<Aabb> bounds_;
};

} // namespace framework::physics
