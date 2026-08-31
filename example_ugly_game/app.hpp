#pragma once

namespace game {

struct DemoOptions {
    const char* dir = nullptr;      // куда писать кадры; nullptr — не писать ни одного
    const char* golden = nullptr;   // с чем сверять последний кадр
    int frames = 300;
    bool update = false;            // испечь эталон вместо сверки
    bool selftest = false;          // два прогона подряд, сверка их между собой
};

int run_window(int frame_cap);
int run_demo(const DemoOptions& opt);

} // namespace game
