#pragma once

#include <span>
#include <string>
#include <vector>

#include "bundle_writer.hpp"

// Режимы `--game` и `--verify-game` игры-образца (спека #29, В2а): таблицу секций приносит
// инструмент игры (`ugly_assetc`), ядро её не знает. У базового `assetc` этих режимов нет.
//
// Бандл игры-образца лежит в git готовым, а перепечь его целиком нигде, кроме машины владельца,
// нельзя: текстурная секция тянет `basisu` и `tint`, которых на раннерах нет. До сверки правка
// `input.txt` или `achievements.txt` без перепекания проезжала молча — бандл принимался на веру,
// хотя на нём стоят и игра-образец, и sim-golden.
//
// Сверяются секции, которые пекутся ЧИСТЫМИ парсерами, без внешнего тулчейна. Текстура остаётся за
// ручной сверкой `bundle_hash` (пункт F в docs/owner-setup.txt) — здесь она недостижима.
namespace asset::game {

// Секция = логическое имя для отчёта + исходник + пекарь. Добавить tool-free секцию — одна строка:
// guid брать неоткуда, кроме самого пекаря, поэтому имя ассета здесь НЕ дублируется.
struct Section {
    const char* label;
    const char* file;
    bool (*bake)(const std::string&, std::vector<AssetInput>&);
};

// `<инструмент> --game <src-dir> <out.bundle> [--basisu P]`: `atlas.png` + секции таблицы.
int run_game(int argc, char** argv, std::span<const Section> sections);
// `<инструмент> --verify-game <src-dir> <bundle>`: 0 — все секции совпали, 1 — нет, 2 — вызов.
int run_verify(int argc, char** argv, std::span<const Section> sections);
// Сама сверка; `tool` называет инструмент в совете перепечь.
bool verify_game_bundle(const std::string& tool, const std::string& src_dir, const std::string& bundle_path,
                        std::span<const Section> sections);

} // namespace asset::game
