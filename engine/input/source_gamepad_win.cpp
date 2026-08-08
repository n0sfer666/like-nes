#include "source.hpp"
#include "codes.hpp"
// windows.h объявляет min/max макросами, и std::min ниже разбирается как обращение к макросу:
// `error C2589: '(': illegal token on right side of '::'`. Определение обязано стоять до include.
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>
#include <algorithm>
#include <cstring>

// Windows native gamepad: XInput (Xbox-класс + вибрация). Диффим против кэша → эмитим изменения.
// Прогнан на живом железе (гейт 8 спеки #14): кнопки, оси и подключение работают, паспорт устройства
// приезжал нулями — отсюда XInputGetCapabilitiesEx ниже.
namespace input {
namespace c = input::code;

namespace {
struct Btn { WORD mask; int code; };
const Btn kBtns[] = {
    {XINPUT_GAMEPAD_A, c::PadA}, {XINPUT_GAMEPAD_B, c::PadB},
    {XINPUT_GAMEPAD_X, c::PadX}, {XINPUT_GAMEPAD_Y, c::PadY},
    {XINPUT_GAMEPAD_LEFT_SHOULDER, c::LB}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, c::RB},
    {XINPUT_GAMEPAD_BACK, c::Back}, {XINPUT_GAMEPAD_START, c::Start},
    {XINPUT_GAMEPAD_LEFT_THUMB, c::LStick}, {XINPUT_GAMEPAD_RIGHT_THUMB, c::RStick},
    {XINPUT_GAMEPAD_DPAD_UP, c::DpUp}, {XINPUT_GAMEPAD_DPAD_DOWN, c::DpDown},
    {XINPUT_GAMEPAD_DPAD_LEFT, c::DpLeft}, {XINPUT_GAMEPAD_DPAD_RIGHT, c::DpRight},
};

// Паспорт устройства (VID/PID) документированный XInput не отдаёт ВОВСЕ — XINPUT_CAPABILITIES знает
// только класс. Профиль пада выбирается по паспорту, поэтому без него любой пад на Windows приезжал
// как 'generic', тогда как на Linux тот же самый экземпляр evdev отдаёт как 045e:0b12 и профиль
// находится. Настоящие VID/PID есть у XInputGetCapabilitiesEx, но xinput1_4.dll экспортирует её
// БЕЗ ИМЕНИ, только по ordinal 108 — слинковать такое нечем, берётся через GetProcAddress. Тем же
// путём ходят SDL и GLFW: единственная альтернатива это сопоставление слотов XInput с устройствами
// Raw Input, а оно само по себе источник ошибок (соответствие ниоткуда не следует).
struct XInputCapabilitiesEx {
    XINPUT_CAPABILITIES caps;
    WORD vid;
    WORD pid;
    WORD product_version;
    DWORD unknown;
};
using CapsExFn = DWORD(WINAPI*)(DWORD, DWORD, DWORD, XInputCapabilitiesEx*);

// Динамически берётся ТОЛЬКО она: горячий XInputGetState остаётся статически слинкованным с
// Xinput9_1_0, который есть на любой Windows. Перевести на указатели и его значило бы поставить
// КАЖДЫЙ кадр в зависимость от успеха LoadLibrary ради поля, которое читается один раз на
// подключение. Индекс слота — понятие системное, а не принадлежащее DLL, поэтому две версии
// библиотеки в одном процессе говорят об одном и том же устройстве.
CapsExFn caps_ex_fn() {
    // Разрешение однократное, и неудача кэшируется так же, как успех: на системе без 1_4 (или с
    // запрещённой загрузкой) иначе каждое подключение пада заново дёргало бы LoadLibrary.
    static const CapsExFn fn = []() -> CapsExFn {
        HMODULE dll = LoadLibraryW(L"xinput1_4.dll");
        if (dll == nullptr) return nullptr;
        return reinterpret_cast<CapsExFn>(GetProcAddress(dll, MAKEINTRESOURCEA(108)));
    }();
    return fn;
}

class WinGamepadSource : public GamepadSource {
public:
    bool init() override { return true; }

    void poll(InputEngine& e) override {
        for (int i = 0; i < 4 && i < MAX_DEVICES; ++i) {
            XINPUT_STATE st{};
            bool ok = XInputGetState(i, &st) == ERROR_SUCCESS;
            if (ok && !active_[i]) { active_[i] = true; btns_[i] = 0; for (int a = 0; a < PAD_AXES; ++a) axes_[i][a] = 0; e.post({RawKind::DeviceConnected, DeviceKind::Gamepad, (uint8_t)i, 0, 0, seq_++}); }
            if (!ok && active_[i]) { active_[i] = false; e.post({RawKind::DeviceDisconnected, DeviceKind::Gamepad, (uint8_t)i, 0, 0, seq_++}); continue; }
            if (!ok) continue;
            const XINPUT_GAMEPAD& g = st.Gamepad;
            for (const Btn& b : kBtns) emit_btn(e, i, b.code, (g.wButtons & b.mask) != 0);
            // Y инвертируется: у XInput +Y это ВВЕРХ, у нашего контракта (codes.hpp) — вниз.
            // Без минуса стик уезжает вертикально наоборот ровно на одной этой платформе.
            emit_axis(e, i, c::LX, g.sThumbLX / 32768.0f); emit_axis(e, i, c::LY, -g.sThumbLY / 32768.0f);
            emit_axis(e, i, c::RX, g.sThumbRX / 32768.0f); emit_axis(e, i, c::RY, -g.sThumbRY / 32768.0f);
            emit_axis(e, i, c::LT, g.bLeftTrigger / 255.0f); emit_axis(e, i, c::RT, g.bRightTrigger / 255.0f);
        }
    }

    void set_rumble(int slot, float low, float high, int) override {
        if (slot < 0 || slot >= 4) return;
        XINPUT_VIBRATION v{};
        v.wLeftMotorSpeed = (WORD)(low * 65535.0f);
        v.wRightMotorSpeed = (WORD)(high * 65535.0f);
        XInputSetState(slot, &v);
    }

    const char* backend_name() const override { return "XInput (Windows)"; }

    // Паспорт: VID/PID через ordinal 108, класс — из тех же capabilities. Имя остаётся производным
    // от КЛАССА, а не выдумывается по вендору: на Windows профиль теперь находится по VID, и врать
    // именем («Xbox Controller») больше незачем — оно и было бы неправдой для всякого не-Xbox пада,
    // который драйвер показывает через XInput.
    PadInfo pad_info(int slot) const override {
        PadInfo info;
        if (slot < 0 || slot >= 4) return info;
        XINPUT_CAPABILITIES caps{};
        bool have_caps = false;
        const CapsExFn ex_fn = caps_ex_fn();
        if (ex_fn != nullptr) {
            XInputCapabilitiesEx ex{};
            if (ex_fn(1, static_cast<DWORD>(slot), 0, &ex) == ERROR_SUCCESS) {
                info.vid = ex.vid;
                info.pid = ex.pid;
                caps = ex.caps;
                have_caps = true;
            }
        }
        // Ordinal 108 недокументирован, поэтому у него ДВА способа подвести: не разрешиться и
        // разрешиться в функцию с иной семантикой. Первый откат был написан только на первый, и
        // отказ разрешившегося вызова отдавал пустой паспорт — то есть подключённый пад терял и
        // имя, и класс, которые до появления VID/PID у него были. Пусто теперь только когда
        // провалились ОБА: документированный XInputGetCapabilities даёт класс, по которому профиль
        // ищется подстрокой имени, как искался раньше.
        if (!have_caps && XInputGetCapabilities(slot, 0, &caps) != ERROR_SUCCESS) return info;
        const char* kind = "XInput gamepad";
        switch (caps.SubType) {
        case XINPUT_DEVSUBTYPE_WHEEL:       kind = "XInput wheel"; break;
        case XINPUT_DEVSUBTYPE_ARCADE_STICK: kind = "XInput arcade stick"; break;
        case XINPUT_DEVSUBTYPE_FLIGHT_STICK: kind = "XInput flight stick"; break;
        case XINPUT_DEVSUBTYPE_DANCE_PAD:   kind = "XInput dance pad"; break;
        case XINPUT_DEVSUBTYPE_GUITAR:      kind = "XInput guitar"; break;
        case XINPUT_DEVSUBTYPE_DRUM_KIT:    kind = "XInput drum kit"; break;
        default: break;
        }
        // Не strncpy: MSVC помечает всё семейство str*cpy как C4996 «may be unsafe», и под /W4 /WX
        // это ошибка сборки. Глушить её через _CRT_SECURE_NO_WARNINGS значило бы снять диагностику
        // со всего TU разом. Длина считается явно, терминатор уже на месте — name[64] = {}.
        const size_t n = std::min(std::strlen(kind), sizeof(info.name) - 1);
        std::memcpy(info.name, kind, n);
        return info;
    }

private:
    void emit_btn(InputEngine& e, int slot, int code, bool pressed) {
        bool was = (btns_[slot] >> code) & 1u;
        if (pressed == was) return;
        if (pressed) btns_[slot] |= (1u << code); else btns_[slot] &= ~(1u << code);
        e.post({pressed ? RawKind::PadButtonDown : RawKind::PadButtonUp, DeviceKind::Gamepad, (uint8_t)slot, (uint16_t)code, 0, seq_++});
    }
    void emit_axis(InputEngine& e, int slot, int code, float v) {
        int32_t raw = fix32::from_float(v).raw;
        if (raw == axes_[slot][code]) return;
        axes_[slot][code] = raw;
        e.post({RawKind::PadAxis, DeviceKind::Gamepad, (uint8_t)slot, (uint16_t)code, raw, seq_++});
    }

    bool active_[MAX_DEVICES] = {};
    uint32_t btns_[MAX_DEVICES] = {};
    int32_t axes_[MAX_DEVICES][PAD_AXES] = {};
    uint64_t seq_ = 1'000'000;
};
} // namespace

GamepadSource* make_gamepad_source() { static WinGamepadSource s; return &s; }

} // namespace input
