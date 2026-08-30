#pragma once
#include <cstdint>

#include "atlas_read.hpp"
#include "clip.hpp"
#include "fixmath.hpp"

// Отладочная отрисовка (спека #17, вертикаль 1, шаг E): линия, рамка, заливка и текст поверх сцены —
// общий инструмент для #15/#16 и игры. Здесь ТОЛЬКО построитель: он превращает примитивы в поток
// квадов и не знает ни про RHI, ни про батч, ни про UV. Отправка в GPU остаётся в игре-образце.
//
// Числа — `fix32`, а не float, по той же причине, что у камеры: поток квадов сворачивается в голден,
// который обязан совпасть на трёх ОС. Поворот отдаётся ЕДИНИЧНЫМ НАПРАВЛЕНИЕМ, а не углом: угол
// потребовал бы atan2, которого в целочисленной математике нет вовсе, а потребителю всё равно нужна
// матрица — он строит её из (dx, dy) без единого тригонометрического вызова.
//
// Буфер квадов ПРИНАДЛЕЖИТ ВЫЗЫВАЮЩЕМУ (инвариант 3 спеки: zero-alloc в кадре). Переполнение
// СЧИТАЕТСЯ, а не молчит: оверлей, тихо обрезающий хвост, показывает неполную картину и читается
// как «эти примитивы не рисовались».
namespace framework::graphics {

using framework::Vec2;

// Цвет упакован в 0xRRGGBBAA: отладке хватает восьми бит на канал, а одно число входит в свёртку
// голдена без разбора на компоненты.
struct DebugQuad {
    Vec2 center{};
    Vec2 half{};
    Vec2 dir{fix32::from_int(1), fix32{}};
    uint32_t rgba = 0xffffffffu;
    RegionId region = 0;
};

// Глифы разрешаются ПО ИМЕНИ и ОДИН РАЗ (шаг D): имя переживает перепаковку страницы, номер — нет.
// `has_text` отдельным полем, потому что атлас без шрифта — законный атлас: у оверлея просто не
// будет текста, и это не повод отказывать в линиях и рамках.
struct DebugGlyphs {
    RegionId solid = 0;
    RegionId digit[10]{};
    RegionId letter[26]{};
    bool has_solid = false;
    bool has_text = false;
};

bool resolve_debug_glyphs(const AtlasTable& table, DebugGlyphs& out);

class DebugDraw {
public:
    DebugDraw(DebugQuad* storage, uint32_t capacity, const DebugGlyphs& glyphs);

    void clear();

    // Линия ЛЮБОГО направления: квад с центром в середине отрезка, длиной в его длину и толщиной
    // поперёк. Отрезок нулевой длины не рисуется вовсе — направление у него не определено, и
    // «квад нулевой ширины» отличался бы от пропуска только тем, что попадает в поток.
    void line(Vec2 a, Vec2 b, fix32 thickness, uint32_t rgba);

    // Рамка: ВНЕШНЯЯ граница совпадает с прямоугольником, толщина уходит внутрь. Вертикальные
    // перекладины укорочены на толщину, поэтому угол покрыт РОВНО ОДИН раз — под полупрозрачным
    // цветом двойное покрытие видно глазом, а не только в тесте.
    void frame(Vec2 center, Vec2 half, fix32 thickness, uint32_t rgba);

    void fill(Vec2 center, Vec2 half, uint32_t rgba);

    // Текст: первый глиф центрируется в `at`, шаг — `cell` вправо. Строчные приводятся к верхнему
    // регистру, всё прочее (пробел, знаки) сдвигает курсор и не даёт квада.
    void text(const char* s, Vec2 at, fix32 cell, uint32_t rgba);

    uint32_t count() const { return count_; }
    const DebugQuad* data() const { return storage_; }
    // Невыданные квады: и те, что не влезли, и те, для которых в атласе нет глифа. Случаи сведены в
    // один счётчик намеренно — снаружи они неразличимы, на экране пусто одинаково.
    uint32_t dropped() const { return dropped_; }

private:
    void push(Vec2 center, Vec2 half, Vec2 dir, uint32_t rgba, RegionId region);

    DebugQuad* storage_ = nullptr;
    uint32_t capacity_ = 0;
    uint32_t count_ = 0;
    uint32_t dropped_ = 0;
    DebugGlyphs glyphs_{};
};

} // namespace framework::graphics
