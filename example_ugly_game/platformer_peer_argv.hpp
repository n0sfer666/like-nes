#pragma once

#include <cstdio>
#include <cstring>
#include <string>

#include "platformer_peer.hpp"

// Разбор служебного хвоста `--peer send|recv <bundle> <prefix> [флаги]` (спека #22, шаг C).
//
// Отдельным заголовком, а не функцией внутри гейта: ролей у двоичного файла две, а целей, у которых
// есть роль пира, стало две — гейт знакомства по петле и гейт прямой адресации. Копия разбора в
// каждой означала бы, что флаг, добавленный одной, второй молча игнорируется: пир принял бы аргумент
// как неизвестное слово, прогон прошёл бы ЗЕЛЁНЫМ и проверил бы не то, что названо в его имени.
//
// Разбор СТРОГИЙ, и это не вкус. Аргументы гейта строит код, но аргументы §14 руководства владелец
// печатает руками на двух машинах: `--lisen 7777` при мягком разборе не был бы назван никем, пир
// ушёл бы знакомиться файлом, которого на второй машине нет, и отдал бы дедлайн вместо «я не понял
// слова». Тот же класс — `--listen abc`: `atol` вернул бы 0, прямая адресация не взвелась бы, и
// молчаливый откат к файлам случился бы ровно там, где весь раунд его запрещает.
namespace platformer {

inline bool is_peer_argv(int argc, char** argv) {
    return argc >= 5 && std::strcmp(argv[1], "--peer") == 0;
}

// Число целиком, а не «сколько разобралось»: хвост после цифр (`7777x`, `70000`) — это опечатка
// владельца, и принять её значило бы слушать не тот порт, о котором просили.
inline bool parse_u32(const char* s, uint32_t lo, uint32_t hi, uint32_t& out) {
    if (s == nullptr || *s == '\0') return false;
    uint64_t v = 0;
    for (const char* c = s; *c != '\0'; ++c) {
        if (*c < '0' || *c > '9') return false;
        v = v * 10 + static_cast<uint64_t>(*c - '0');
        if (v > hi) return false;
    }
    if (v < lo) return false;
    out = static_cast<uint32_t>(v);
    return true;
}

inline bool refuse(const char* what, const char* token) {
    std::printf("peer: %s (%s)\n", what, token);
    return false;
}

// Пары «флаг — значение» разбираются по одному месту, потому что забытое значение последнего флага
// (`--deaf 120` без второго числа) при проверке `i + n < argc` просто выпадало бы из разбора молча.
inline bool parse_peer(int argc, char** argv, PeerConfig& cfg) {
    if (std::strcmp(argv[2], "send") == 0) cfg.sender = true;
    else if (std::strcmp(argv[2], "recv") != 0) return refuse("the role is neither send nor recv", argv[2]);
    cfg.bundle = argv[3];
    cfg.prefix = argv[4];
    for (int i = 5; i < argc; ++i) {
        const char* flag = argv[i];
        const int left = argc - i - 1;
        uint32_t v = 0;
        if (std::strcmp(flag, "--drop") == 0 && left >= 1) {
            if (!parse_u32(argv[++i], 0, 0xffffffu, v)) return refuse("--drop wants a tick", argv[i]);
            cfg.drop_tick = static_cast<int64_t>(v);
        } else if (std::strcmp(flag, "--deaf") == 0 && left >= 2) {
            if (!parse_u32(argv[i + 1], 0, 0xffffffu, cfg.deaf_at) ||
                !parse_u32(argv[i + 2], 0, 0xffffffu, cfg.deaf_ms))
                return refuse("--deaf wants a tick and a duration", argv[i + 1]);
            i += 2;
        } else if (std::strcmp(flag, "--listen") == 0 && left >= 1) {
            // Порт свой, номер известен заранее и обеим сторонам. Эфемерный тут не годится по
            // построению: его номер назначает ядро уже после старта, а сообщить его соседу на
            // другой машине нечем — файла у них общего нет, ради чего вся эта форма и заведена.
            if (!parse_u32(argv[++i], 1, 65535, v)) return refuse("--listen wants a port", argv[i]);
            cfg.listen_port = static_cast<uint16_t>(v);
        } else if (std::strcmp(flag, "--at") == 0 && left >= 1) {
            cfg.peer_at = argv[++i];
        } else {
            return refuse("this argument was not understood", flag);
        }
    }
    return true;
}

} // namespace platformer
