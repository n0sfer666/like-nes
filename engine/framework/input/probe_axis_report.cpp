#include "probe_axis_report.hpp"

#include <cstdio>

#include "codes.hpp"

namespace framework::input {
namespace {

// Порог «стик отклонён», а не «стик дрогнул»: мёртвая зона профиля 0.18, и сообщение об
// отклонении обязано означать осознанное движение, иначе первая же наводка помечает все четыре
// направления пройденными до того, как владелец коснулся пада.
constexpr double DEFLECTED = 0.5;
// Пол шума покоящегося стика. Точное сравнение с нулём здесь уже подводило: живой XInput отдавал
// `raw lx [-0.01,+0.00]` на нетронутом паде, и вердикт переворачивался с «бэкенд молчит» на
// «пресет съел стик» — то есть обвинял раскладку по шуму драйвера. Порог ниже самой маленькой
// мёртвой зоны в профилях (0.10), поэтому настоящее отклонение он не проглотит.
constexpr double NOISE = 0.05;

const char* const DIR_NAME[] = {"right", "left", "up", "down"};
// Контракт знаков — codes.hpp: сырая ось +Y это ВНИЗ, а пресет переворачивает её (`padaxis:-ly`),
// поэтому у разрешённого move_y плюс это ВВЕРХ. Ожидание печатается рядом со значением: шаг гейта
// ловит именно расхождение бэкендов по знаку, и владельцу не с чем сверять число без него.
const char* const DIR_RULE[] = {"move_x must read > 0", "move_x must read < 0",
                                "move_y must read > 0", "move_y must read < 0"};

void track(double v, double& lo, double& hi) {
    if (v < lo) lo = v;
    if (hi < v) hi = v;
}

// Наибольшее отклонение по модулю. Ноль возвращается ПОЛОЖИТЕЛЬНЫМ: `-lo` от нулевого минимума
// даёт -0.0, и отчёт печатал `max |raw| = -0.00` — модуль с минусом читается как опечатка кода.
double span(const double lo, const double hi) {
    const double m = -lo < hi ? hi : -lo;
    return m == 0 ? 0.0 : m;
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
    // Направление задаёт СЫРАЯ ось (raw +Y — вниз), ответ проверяется по разрешённой.
    const bool now[DIRS] = {raw[0] > DEFLECTED, raw[0] < -DEFLECTED, raw[1] < -DEFLECTED,
                            raw[1] > DEFLECTED};
    const bool ok[DIRS] = {res[0] > 0, res[0] < 0, res[1] > 0, res[1] < 0};
    const bool flip[DIRS] = {res[0] < 0, res[0] > 0, res[1] < 0, res[1] > 0};
    for (int d = 0; d < DIRS; ++d) {
        if (!now[d]) continue;
        if (ok[d]) answered_[d] = true;
        if (flip[d]) inverted_[d] = true;
        if (deflected_[d]) continue;
        deflected_[d] = true;
        // Перевод строки перед сообщением — обязателен: строка состояния живёт на `\r`, и без него
        // отчёт лёг бы поверх неё и был бы затёрт следующим же кадром.
        std::printf("\n[probe] stick %s: raw lx=%+.2f ly=%+.2f -> move=(%+.2f,%+.2f)  [%s]\n",
                    DIR_NAME[d], raw[0], raw[1], res[0], res[1], DIR_RULE[d]);
    }
}

void AxisWitness::report() const {
    std::printf("\n[probe] axis report (slot 0, whole session):\n"
                "        raw stick   lx [%+.2f,%+.2f]  ly [%+.2f,%+.2f]\n"
                "        move (any source: stick OR keyboard)"
                "   x [%+.2f,%+.2f]  y [%+.2f,%+.2f]\n",
                raw_lo_[0], raw_hi_[0], raw_lo_[1], raw_hi_[1], res_lo_[0], res_hi_[0], res_lo_[1],
                res_hi_[1]);
    std::printf("        stick pushed:");
    for (int d = 0; d < DIRS; ++d)
        std::printf(" %s=%s", DIR_NAME[d],
                    !deflected_[d]  ? "NO"
                    : answered_[d]  ? "yes"
                    : inverted_[d]  ? "INVERTED"
                                    : "EATEN");
    std::printf("\n");

    const double reach = span(raw_lo_[0], raw_hi_[0]) < span(raw_lo_[1], raw_hi_[1])
                             ? span(raw_lo_[1], raw_hi_[1])
                             : span(raw_lo_[0], raw_hi_[0]);
    int pushed = 0, eaten = 0, flipped = 0;
    for (int d = 0; d < DIRS; ++d) {
        pushed += deflected_[d] ? 1 : 0;
        if (!deflected_[d] || answered_[d]) continue;
        ++(inverted_[d] ? flipped : eaten);
    }
    // Молчание тут читалось бы как «к осям нет вопросов», поэтому пустой прогон называет себя сам —
    // то же основание, что у позитивного контроля греп-гейтов в tree_invariants.sh.
    if (reach <= NOISE)
        std::printf("        the backend delivered no stick value above idle noise (max |raw| =\n"
                    "        %.2f) this session: either the stick was never pushed, or this\n"
                    "        backend does not report axes at all.\n",
                    reach);
    else if (pushed == 0)
        std::printf("        the stick moved (max |raw| = %.2f) but never far enough to count as\n"
                    "        a deflection: push it to the rim, one direction at a time.\n",
                    reach);
    else if (flipped > 0)
        std::printf("        %d direction(s) marked INVERTED: the stick resolved with the sign\n"
                    "        opposite to the contract in codes.hpp (+X right, raw +Y down, and\n"
                    "        `padaxis:-ly` flipping move_y once). This is the per-platform defect\n"
                    "        this step exists for - name the OS and the direction when reporting.\n",
                    flipped);
    else if (eaten > 0)
        std::printf("        %d direction(s) marked EATEN: the backend DID deliver the stick and\n"
                    "        the preset resolved nothing from it - the binding or the deadzone,\n"
                    "        not the driver.\n",
                    eaten);
}

} // namespace framework::input
