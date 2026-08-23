#pragma once
#include <cstdint>

// Zero-parse раскладка тайловой карты: `assetc` печёт её из текстового исходника, рантайм читает
// прямо из mmap-региона бандла — тем же швом, что таблица профилей движения (#16), пресеты ввода
// (#14) и достижения (#10). Причина та же и здесь главная: РАСКЛАДКА УРОВНЯ правится десятками
// итераций подряд, и пересборка движка на каждый передвинутый тайл убивает сам цикл подбора.
//
// Величины лежат СЫРЫМ Q16.16 (`fix32::raw`), а не числами с плавающей точкой: иначе таблица
// зависела бы от округления на машине, которая её пекла, и байт-детерминизм бейка (условие #5)
// держался бы на совпадении FPU, а не на арифметике.
//
// Карт в секции НЕСКОЛЬКО, как профилей в `movement`: имя нужно, чтобы уровень выбирался данными, а
// не отдельным guid на комнату, — иначе каждая новая комната требовала бы правки пекаря. Флаги
// лежат ПЛОТНЫМ массивом `TileFlags` по строкам (`y * width + x`), тем же порядком, что в
// `TileGrid`: читатель копирует блок целиком, и порядок, разошедшийся с сеткой, дал бы карту,
// прочитанную без ошибки и означающую другое.
namespace framework::tilemap {

constexpr uint8_t MAP_MAGIC[4] = {'L', 'N', 'T', 'M'};   // like-nes tilemap
constexpr uint32_t MAP_VERSION = 1;

struct MapHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t map_count;
    uint32_t maps_offset;
    uint32_t strings_offset;
    uint32_t total_size;
};
static_assert(sizeof(MapHeader) == 24, "MapHeader layout pinned (zero-parse ABI)");

// Размер блока флагов в записи НЕ лежит: он равен `width * height * sizeof(TileFlags)` и вычисляется
// читателем при проверке границ. Хранить его значило бы завести второй источник правды о том же
// числе — и первое же расхождение между ними пришлось бы разрешать выбором, у которого нет
// основания.
struct MapRow {
    uint32_t name_offset;
    int32_t origin_x_raw;
    int32_t origin_y_raw;
    int32_t tile_size_raw;
    uint32_t width;
    uint32_t height;
    uint32_t tiles_offset;
};
static_assert(sizeof(MapRow) == 28, "MapRow layout pinned (zero-parse ABI)");

} // namespace framework::tilemap
