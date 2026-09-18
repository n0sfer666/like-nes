#pragma once
#include "scene.hpp"
#include "serialize.hpp"
#include <cstdint>
#include <cstdio>
#include <string>

// Общее для двух гейтов отказа — `scene_refusal_test` (файл сцены) и `scene_undo_refusal_test`
// (снимок undo). Общее у них ровно это: счёт провалов, поиск причины в тексте, проверка цитаты на
// UTF-8 и ФИКСТУРЫ, взятые у самого сериализатора. Сами утверждения не общие и общими не будут: у
// файла четырнадцать причин и своя шапка, у снимка — причина 7 и 9–14, шапки и строк `E` у него нет
// (аудит #21, A·2·8).
namespace ide::refusal {

inline int fails = 0;

inline void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

inline bool says(const std::string& why, const std::string& needle) {
    return why.find(needle) != std::string::npos;
}

inline int report(const char* gate) {
    std::printf("%s: %s\n", gate, fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

// Диагностика уезжает в лог и в интерфейс редактора: обрезанная посреди символа или сложенная из
// чужих байтов, она рвёт кодировку всей строки, куда её вставили. Спрашивают отсюда ОБА гейта — и
// про рез длинной строки, и про сырьё не в UTF-8. Проверка своя, а не через ОС: гейт обязан про
// БАЙТЫ, которые сложил разбор, одинаково на трёх системах. Строгая, а не «по старшим битам»:
// пересортица (`C0 80` вместо `00`), суррогаты (D800..DFFF) и всё выше U+10FFFF в UTF-8
// запрещены, и приняв их, проверка молча пропустила бы обрезку в середине символа, если та
// оставила бы синтаксически похожий хвост (ревью аудита #21, A·2·8).
inline bool valid_utf8(const std::string& s) {
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3
                                                          : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (len == 0 || i + len > s.size()) return false;
        uint32_t cp = len == 1 ? c : (c & (0xFFu >> (len + 1)));
        for (size_t k = 1; k < len; ++k) {
            const unsigned char t = static_cast<unsigned char>(s[i + k]);
            if ((t & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (t & 0x3F);
        }
        static const uint32_t MIN[5] = {0, 0, 0x80, 0x800, 0x10000};
        if (cp < MIN[len] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        i += len;
    }
    return true;
}

// Фикстуры берутся у самого сериализатора — источник правды тот же, что у гейта 1. Раньше шапка
// была написана руками как `C Name {"n":"hero"}`, текста, которого `serialize` не пишет НИКОГДА
// (член зовётся `value`), то есть контроль «целое читается» стоял на битом тексте и держал ровно
// тот дефект, против которого заведён гейт (ревью аудита #21, A·2·8).
inline std::string head_text() {
    Scene t;
    t.create(10).set<Name>({"hero"});
    return serialize(t);
}

// Строки `C ...` одной сущности, без `E`: и тело снимка undo, и законный хвост файла сцены.
inline std::string pos_lines() {
    Scene t;
    t.create(1).set<Position>({fix32::from_int(3), fix32::from_int(5)});
    return serialize_entity(t, 1);
}

} // namespace ide::refusal
