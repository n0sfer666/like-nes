#pragma once

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "platform_fs.hpp"

// Как два процесса находят порты друг друга (спека #22, шаг C): каждый привязывается к ЭФЕМЕРНОМУ
// порту петли и кладёт его номер в файл, второй этот файл опрашивает.
//
// Фиксированные номера портов не годятся: раннер гоняет джобы соседями, и занятый порт превратил бы
// гейт в проверку того, что сегодня никто другой его не взял. Эфемерный порт назначает ядро, а
// значит его надо кому-то сообщить — отсюда файл.
namespace platformer::rendezvous {

// '\n' пишется последним и служит признаком дописанного файла: сосед опрашивает его, пока мы пишем,
// и обрыв на середине числа обязан читаться как «ещё не готово», а не как порт 12.
inline bool publish(const std::string& path, uint16_t port) {
    std::FILE* f = platform::open_file(path, "wb");
    if (f == nullptr) return false;
    const bool ok = std::fprintf(f, "%u\n", static_cast<unsigned>(port)) > 0;
    return std::fclose(f) == 0 && ok;
}

inline bool read(const std::string& path, uint16_t& out) {
    std::string text;
    if (!platform::read_text(path, text)) return false;
    unsigned long v = 0;
    size_t i = 0;
    for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) {
        v = v * 10 + static_cast<unsigned long>(text[i] - '0');
        if (v > 65535) return false;
    }
    if (i == 0 || i >= text.size() || text[i] != '\n' || v == 0) return false;
    out = static_cast<uint16_t>(v);
    return true;
}

// Порт соседа появляется, когда сосед стартовал: запускает их родитель по одному, и второй процесс
// может отстать на секунды под нагрузкой раннера. Потолок ЗАДАЁТ ВЫЗЫВАЮЩИЙ и берёт его не с
// потолка: ожидание ребёнка в шве конечно (`Child::wait` на Windows — 30 с), и бюджет, не
// уместившийся под ним, означал бы, что живого пира убивает родитель, а заявленные коды отказа не
// наблюдаются никогда.
inline bool await(const std::string& path, uint16_t& out, int64_t timeout_ms) {
    constexpr int64_t STEP_MS = 100;
    for (int64_t waited = 0; waited < timeout_ms; waited += STEP_MS) {
        if (read(path, out)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(STEP_MS));
    }
    // Последний взгляд после сна: иначе решение «не дождался» принимается по состоянию, которое
    // устарело на целый шаг ожидания.
    return read(path, out);
}

} // namespace platformer::rendezvous
