#include "probe_axis_report.hpp"

#include <cstdio>

#include "codes.hpp"

namespace framework::input {
namespace {

// Порог «стик отклонён», а не «стик дрогнул»: мёртвая зона профиля 0.18, и сообщение об
// отклонении обязано означать осознанное движение, иначе первая же наводка помечает все четыре
// направления пройденными до того, как владелец коснулся пада.
constexpr double DEFLECTED = 0.5;

const char* const DIR_NAME[] = {"right", "left", "up", "down"};
// Контракт знаков — codes.hpp: сырая ось +Y это ВНИЗ, а пресет переворачивает её (`padaxis:-ly`),
// поэтому у разрешённого move_y плюс это ВВЕРХ. Ожидание печатается рядом со значением: шаг гейта
// ловит именно расхождение бэкендов по знаку, и владельцу не с чем сверять число без него.
const char* const DIR_RULE[] = {"right must read move_x > 0", "left must read move_x < 0",
                                "up must read move_y > 0", "down must read move_y < 0"};

void track(double v, double& lo, double& hi) {
    if (v < lo) lo = v;
    if (hi < v) hi = v;
}

} // namespace

void AxisWitness::observe(const ::input::DeviceState& dev, const ::input::InputFrame& frame) {
    namespace c = ::input::code;
    const double raw[2] = {dev.pad_axis(0, c::LX).to_double(), dev.pad_axis(0, c::LY).to_double()};
    const double res[2] = {frame.axes[0].to_double(), frame.axes[1].to_double()};
    for (int a = 0; a < 2; ++a) {
        track(raw[a], raw_lo_[a], raw_hi_[a]);
        track(res[a], res_lo_[a], res_hi_[a]);
    }
    const bool now[DIRS] = {res[0] > DEFLECTED, res[0] < -DEFLECTED, res[1] > DEFLECTED,
                            res[1] < -DEFLECTED};
    for (int d = 0; d < DIRS; ++d) {
        if (!now[d] || seen_[d]) continue;
        seen_[d] = true;
        // Перевод строки перед сообщением — обязателен: строка состояния живёт на `\r`, и без него
        // отчёт лёг бы поверх неё и был бы затёрт следующим же кадром.
        std::printf("\n[probe] stick %s: raw lx=%+.2f ly=%+.2f -> move=(%+.2f,%+.2f)  [%s]\n",
                    DIR_NAME[d], raw[0], raw[1], res[0], res[1], DIR_RULE[d]);
    }
}

void AxisWitness::report() const {
    std::printf("\n[probe] axis report (slot 0, whole session):\n"
                "        raw    lx [%+.2f,%+.2f]  ly [%+.2f,%+.2f]\n"
                "        move   x  [%+.2f,%+.2f]  y  [%+.2f,%+.2f]\n",
                raw_lo_[0], raw_hi_[0], raw_lo_[1], raw_hi_[1], res_lo_[0], res_hi_[0], res_lo_[1],
                res_hi_[1]);
    std::printf("        directions deflected:");
    for (int d = 0; d < DIRS; ++d) std::printf(" %s=%s", DIR_NAME[d], seen_[d] ? "yes" : "NO");
    std::printf("\n");

    const bool raw_dead = raw_lo_[0] == 0 && raw_hi_[0] == 0 && raw_lo_[1] == 0 && raw_hi_[1] == 0;
    const bool res_dead = res_lo_[0] == 0 && res_hi_[0] == 0 && res_lo_[1] == 0 && res_hi_[1] == 0;
    // Молчание тут читалось бы как «осей нет вопросов», поэтому пустой прогон называет себя сам —
    // то же основание, что у позитивного контроля греп-гейтов в tree_invariants.sh.
    if (raw_dead)
        std::printf("        the backend delivered NO stick value at all this session: either the\n"
                    "        stick was never pushed, or this backend does not report axes.\n");
    else if (res_dead)
        std::printf("        the backend DID deliver stick values, but the preset resolved none of\n"
                    "        them: the binding or the deadzone eats the stick, not the driver.\n");
}

} // namespace framework::input
