#include <chrono>
#include <cstdio>
#include <thread>
#include <GLFW/glfw3.h>
#include "action_map.hpp"
#include "codes.hpp"
#include "engine.hpp"
#include "sim.hpp"
#include "source.hpp"

// Live-демо (owner HW): GLFW-окно (kbd/mouse/трекпад) + native gamepad (GameController/XInput/
// evdev). Печатает InputFrame (Action/оси), логирует hot-plug, триггерит rumble по Jump/R.
// Запуск: ./input_demo   (Esc — выход). Валидирует столп #4 «устройства из коробки».
using namespace input;
namespace c = input::code;

static const char* kActionName[] = {"Jump", "Fire"};
static const char* kAxisName[] = {"MoveX", "MoveY", "AimX", "AimY"};

static ActionMap make_map() {
    ActionMap m;
    m.bind(A_Jump, {SourceKind::Key, c::Space, 1});
    m.bind(A_Jump, {SourceKind::PadButton, c::PadA, 1});
    m.bind(A_Fire, {SourceKind::MouseButton, c::MLeft, 1});
    m.bind(A_Fire, {SourceKind::PadButton, c::PadB, 1});
    m.bind_axis(AX_MoveX, {SourceKind::Key, c::D, 1}, {SourceKind::Key, c::A, 1}, fix32{}, 0);
    m.bind_axis(AX_MoveX, {SourceKind::PadAxis, c::LX, 1}, {SourceKind::None, 0, 0}, fix32::from_float(0.15), 0);
    m.bind_axis(AX_MoveY, {SourceKind::Key, c::W, 1}, {SourceKind::Key, c::S, 1}, fix32{}, 0);
    // sign -1: клавиша W выше даёт +MoveY (вверх), а сырая LY по контракту codes.hpp растёт ВНИЗ —
    // без инверсии пад и клавиатура в одном и том же демо ехали бы в разные стороны.
    m.bind_axis(AX_MoveY, {SourceKind::PadAxis, c::LY, -1}, {SourceKind::None, 0, 0}, fix32::from_float(0.15), 0);
    // Мышь/трекпад: scale 1/32 → дельта видна пропорционально (не мгновенный clamp до ±1). 2D-aim.
    m.bind_axis(AX_AimX, {SourceKind::MouseAxis, c::MAxX, 1}, {SourceKind::None, 0, 0}, fix32{}, 0, fix32::from_float(1.0 / 32));
    m.bind_axis(AX_AimX, {SourceKind::PadAxis, c::RX, 1}, {SourceKind::None, 0, 0}, fix32::from_float(0.15), 0);
    m.bind_axis(AX_AimY, {SourceKind::MouseAxis, c::MAxY, 1}, {SourceKind::None, 0, 0}, fix32{}, 0, fix32::from_float(1.0 / 32));
    m.bind_axis(AX_AimY, {SourceKind::PadAxis, c::RY, 1}, {SourceKind::None, 0, 0}, fix32::from_float(0.15), 0);
    PlayerAssign pa; pa.use_kbd_mouse = true; pa.pad_slot = 0; m.assign_player(0, pa);
    return m;
}

static void print_frame(const InputFrame& f) {
    printf("\r[t%6u] ", f.tick);
    for (int a = 0; a < 2; ++a) printf("%s:%s ", kActionName[a], f.action_held(a) ? "#" : ".");
    for (int a = 0; a < 4; ++a) printf("%s:%+.2f ", kAxisName[a], f.axes[a].to_double());
    printf("mEv:%llu   ", (unsigned long long)glfw_mouse_event_count());
    fflush(stdout);
}

int main() {
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // ввод без граф-контекста
    GLFWwindow* win = glfwCreateWindow(640, 360, "like-nes input PoC (Esc=quit)", nullptr, nullptr);
    if (!win) { fprintf(stderr, "window failed\n"); glfwTerminate(); return 1; }

    // Raw relative motion: захват движения мыши/трекпада в сфокусированное окно независимо от
    // позиции курсора и краёв экрана — корректный путь для «aim» (иначе события уходят окну под
    // курсором). Тогл M освобождает курсор (обычный режим).
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    ActionMap map = make_map();
    InputEngine engine(map);
    install_glfw_input(win, engine);
    GamepadSource* pad = make_gamepad_source();
    bool have_pad_backend = pad && pad->init();
    printf("input PoC — kbd/mouse: GLFW window | gamepad: %s\n",
           have_pad_backend ? pad->backend_name() : "none");
    printf("bindings: Space/PadA=Jump  LMB/PadB=Fire  WASD/LStick=Move  Mouse/RStick=Aim  R/Jump=rumble\n");
    printf("mouse CAPTURED (курсор скрыт, aim=трекпад/мышь). M=освободить/захватить, Esc=выход\n");

    bool prev_pad[MAX_DEVICES] = {};
    uint64_t held_prev = 0, dropped_prev = 0;
    bool captured = true, m_prev = false;
    using clock = std::chrono::steady_clock;
    const auto period = std::chrono::microseconds(16667); // 60 Гц fixed timestep
    auto next = clock::now();
    for (uint32_t t = 0; !glfwWindowShouldClose(win); ++t) {
        next += period;
        std::this_thread::sleep_until(next);          // кап частоты: дельта мыши копится за кадр
        glfwPollEvents();                 // kbd/mouse/focus → engine.post
        if (have_pad_backend) pad->poll(engine); // native gamepad → engine.post
        const InputFrame& f = engine.begin_tick(t, 0);

        for (int s = 0; s < MAX_DEVICES; ++s) {
            bool now = engine.device().pad_connected[s];
            if (now != prev_pad[s]) { printf("\n[hotplug] gamepad slot %d %s\n", s, now ? "CONNECTED" : "DISCONNECTED"); prev_pad[s] = now; }
        }
        if (engine.dropped() != dropped_prev) { printf("\n[warn] dropped %llu raw events (queue full)\n", (unsigned long long)engine.dropped()); dropped_prev = engine.dropped(); }
        if (f.action_pressed(A_Jump) && have_pad_backend) pad->set_rumble(0, 0.7f, 0.7f, 200);
        if (glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS && have_pad_backend) pad->set_rumble(0, 1.0f, 1.0f, 150);
        bool m_now = glfwGetKey(win, GLFW_KEY_M) == GLFW_PRESS;
        if (m_now && !m_prev) { captured = !captured; glfwSetInputMode(win, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL); printf("\n[mouse] %s\n", captured ? "CAPTURED" : "released"); }
        m_prev = m_now;
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

        bool axis_active = false;
        for (int a = 0; a < 4; ++a) if (!(f.axes[a] == fix32{})) axis_active = true;
        if (f.held != held_prev || axis_active || (t % 30) == 0) { print_frame(f); held_prev = f.held; }
    }
    printf("\nbye\n");
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
