#include <cstdio>

#include "platform_args.hpp"
#include "platformer_live_hooks.hpp"
#include "platformer_live_input.hpp"
#include "platformer_peer.hpp"
#include "platformer_peer_argv.hpp"
#include "platformer_window.hpp"

// Живая половина гейта 9 спеки #22: тот же пир, что у headless-гейтов, но с окном на ОБЕИХ
// сторонах. Клавиатура — только у отправителя: ввод по проводу идёт в одну сторону, `run_peer`
// спрашивает шов под `if (cfg.sender)`, а получатель доигрывает приехавшее и судит картинку.
//
// Цель отдельная, а цикл общий. Гейтам 1-7 окна не нужно вовсе — они сверяют хеши двух процессов, и
// GLFW с wgpu на раннере им только мешали бы; человеку за клавиатурой не нужны ни `--drop`, ни
// `--deaf`. Общим остаётся `run_peer`: вторая копия цикла отката разошлась бы с первой молча, и
// гейты продолжали бы сходиться на своей, пока владелец играет в другую.
//
// Роль задаётся тем же `--peer send|recv`, что и у гейтов, и разбирается ТЕМ ЖЕ строгим разбором:
// аргументы здесь печатает человек на двух машинах, и `--lisen 7777`, принятое молча, увело бы пира
// знакомиться файлом, которого на второй машине нет.
namespace {

void usage(const char* exe) {
    std::printf("usage: %s --peer send|recv <bundle> <prefix> --listen <port> --at <host:port>\n",
                exe);
    std::printf("  send owns the input, recv watches; both show a window and quit on Esc\n");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (!platformer::is_peer_argv(argc, argv)) {
        usage(argc > 0 ? argv[0] : "game_platformer_net_live");
        return 3;
    }
    platformer::PeerConfig cfg;
    if (!platformer::parse_peer(argc, argv, cfg)) return 3;

    // Окно поднимается ДО знакомства: сосед ждёт порт с дедлайном, и сторона, потратившая эти
    // секунды на создание устройства wgpu, съела бы их из чужого рандеву.
    platformer::Window win;
    if (!win.open(cfg.sender ? "like-nes: peer send" : "like-nes: peer recv")) return 1;
    // Раскладка открывается ТОЛЬКО у отправителя: получатель её не спрашивает ни разу, а открытая
    // и там она валила бы наблюдающую сторону кодом 1 за пад, которого у неё может не быть вовсе.
    platformer::LiveInput in;
    if (cfg.sender) {
        if (!in.open(win.handle(), cfg.bundle)) {
            win.close();
            return 1;
        }
        std::printf("peer send: %u ticks, gamepad backend %s, Esc quits\n", platformer::LIVE_TICKS,
                    in.pad_name());
    } else {
        std::printf("peer recv: %u ticks, watching the other side, Esc quits\n",
                    platformer::LIVE_TICKS);
    }

    platformer::LiveHooks hooks(win, in);
    cfg.hooks = &hooks;
    const int code = platformer::run_peer(cfg);
    win.close();
    return code;
}
