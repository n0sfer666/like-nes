#include <iostream>
#include <sstream>
#include <string>

#include "mobile_app.hpp"
#include "mobile_probe.hpp"

// The Python driver feeds independent raw-axis conversions; the game and TouchSource are real.
bool mobile_cpu_probe(mobile::Game& game) {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string action;
        in >> action;
        if (action == "safe") {
            uint32_t w = 0, h = 0;
            mobile::SafeArea safe;
            int rotation = 0;
            if (!(in >> w >> h >> safe.left >> safe.top >> safe.right >> safe.bottom >> safe.scale >> rotation))
                return false;
            game.resize(w, h, safe);
            std::cout << mobile::safe_line(w, h, safe, rotation, "cpu") << '\n';
        } else {
            intptr_t id = 0;
            float x = 0, y = 0;
            if (!(in >> id >> x >> y)) return false;
            mobile::Touch phase;
            if (action == "down") phase = mobile::Touch::Down;
            else if (action == "move") phase = mobile::Touch::Move;
            else if (action == "up") phase = mobile::Touch::Up;
            else return false;
            game.pointer(id, phase, x, y);
            std::cout << mobile::touch_line(id, phase, x, y, game.touch_state(), "cpu") << '\n';
        }
        std::string extra;
        if (in >> extra) return false;
    }
    return !std::cin.bad();
}
