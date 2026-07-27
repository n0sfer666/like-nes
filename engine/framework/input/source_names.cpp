#include "source_names.hpp"

#include <cstdio>

#include "codes.hpp"

namespace framework::input {
namespace {

using ::input::Source;
using ::input::SourceKind;

struct Named {
    const char* name;
    uint16_t code;
};

// Именованные клавиши. Буквы, цифры и F-ряд сюда НЕ выписаны: они выводятся арифметикой ниже,
// а список из тридцати строк вида {"b", 66} — это пропущенная абстракция, которая расходится с
// кодами при первой же опечатке.
const Named KEYS[] = {
    {"space", 32},  {"esc", 256},   {"enter", 257}, {"tab", 258},  {"backspace", 259},
    {"right", 262}, {"left", 263},  {"down", 264},  {"up", 265},   {"lshift", 340},
    {"lctrl", 341}, {"lalt", 342},  {"rshift", 344}, {"rctrl", 345}, {"ralt", 346},
};

// Нормализованная модель пада: положение на ромбе, а не буква производителя.
const Named PAD_BUTTONS[] = {
    {"south", ::input::code::PadA}, {"east", ::input::code::PadB},
    {"west", ::input::code::PadX},  {"north", ::input::code::PadY},
    {"lb", ::input::code::LB},      {"rb", ::input::code::RB},
    {"back", ::input::code::Back},  {"start", ::input::code::Start},
    {"lstick", ::input::code::LStick}, {"rstick", ::input::code::RStick},
    {"dpup", ::input::code::DpUp},  {"dpright", ::input::code::DpRight},
    {"dpdown", ::input::code::DpDown}, {"dpleft", ::input::code::DpLeft},
};

const Named PAD_AXES[] = {
    {"lx", ::input::code::LX}, {"ly", ::input::code::LY},
    {"rx", ::input::code::RX}, {"ry", ::input::code::RY},
    {"lt", ::input::code::LT}, {"rt", ::input::code::RT},
};

const Named MOUSE_BUTTONS[] = {
    {"left", ::input::code::MLeft}, {"right", ::input::code::MRight},
    {"middle", ::input::code::MMiddle},
};

const Named MOUSE_AXES[] = {
    {"x", ::input::code::MAxX}, {"y", ::input::code::MAxY}, {"wheel", ::input::code::MAxWheel},
};

template <std::size_t N>
bool lookup(const Named (&table)[N], const std::string& name, uint16_t& out) {
    for (const Named& n : table)
        if (name == n.name) { out = n.code; return true; }
    return false;
}

template <std::size_t N>
const char* reverse(const Named (&table)[N], uint16_t code) {
    for (const Named& n : table)
        if (n.code == code) return n.name;
    return nullptr;
}

bool parse_key(const std::string& name, uint16_t& out) {
    if (name.size() == 1) {
        const char c = name[0];
        if (c >= 'a' && c <= 'z') { out = static_cast<uint16_t>(c - 'a' + 'A'); return true; }
        if (c >= '0' && c <= '9') { out = static_cast<uint16_t>(c); return true; }
    }
    if (name.size() >= 2 && name[0] == 'f') {
        int n = 0;
        for (std::size_t i = 1; i < name.size(); ++i) {
            if (name[i] < '0' || name[i] > '9') { n = 0; break; }
            n = n * 10 + (name[i] - '0');
        }
        if (n >= 1 && n <= 12) { out = static_cast<uint16_t>(289 + n); return true; }  // GLFW F1 = 290
    }
    return lookup(KEYS, name, out);
}

std::string key_name(uint16_t code) {
    if (code >= 'A' && code <= 'Z') return std::string(1, static_cast<char>(code - 'A' + 'a'));
    if (code >= '0' && code <= '9') return std::string(1, static_cast<char>(code));
    if (code >= 290 && code <= 301) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "f%d", code - 289);
        return buf;
    }
    const char* n = reverse(KEYS, code);
    return n != nullptr ? n : std::string();
}

} // namespace

bool parse_source(const std::string& text, Source& out) {
    const std::size_t colon = text.find(':');
    if (colon == std::string::npos || colon + 1 >= text.size()) return false;
    const std::string prefix = text.substr(0, colon);
    std::string name = text.substr(colon + 1);

    // Знак перед именем оси — инверсия направления, а не отдельный источник: `padaxis:-ly`
    // читается как «ось LY, вверх положительно», и без него каждая ось требовала бы пары имён.
    int8_t sign = 1;
    if (name[0] == '-') {
        sign = -1;
        name = name.substr(1);
        if (name.empty()) return false;
    }

    uint16_t code = 0;
    if (prefix == "key") {
        if (!parse_key(name, code)) return false;
        out = {SourceKind::Key, code, sign};
        return true;
    }
    if (prefix == "mouse") {
        if (!lookup(MOUSE_BUTTONS, name, code)) return false;
        out = {SourceKind::MouseButton, code, sign};
        return true;
    }
    if (prefix == "mouseaxis") {
        if (!lookup(MOUSE_AXES, name, code)) return false;
        out = {SourceKind::MouseAxis, code, sign};
        return true;
    }
    if (prefix == "pad") {
        if (!lookup(PAD_BUTTONS, name, code)) return false;
        out = {SourceKind::PadButton, code, sign};
        return true;
    }
    // `trigger:` — синоним `padaxis:` для LT/RT: у триггера своя форма отклика (порог вместо
    // мёртвой зоны), и имя в манифесте обязано это показывать.
    if (prefix == "padaxis" || prefix == "trigger") {
        if (!lookup(PAD_AXES, name, code)) return false;
        out = {SourceKind::PadAxis, code, sign};
        return true;
    }
    return false;
}

std::string source_name(const Source& src) {
    const std::string sign = src.sign < 0 ? "-" : "";
    switch (src.kind) {
    case SourceKind::Key: {
        const std::string n = key_name(src.code);
        return n.empty() ? std::string() : "key:" + n;
    }
    case SourceKind::MouseButton: {
        const char* n = reverse(MOUSE_BUTTONS, src.code);
        return n != nullptr ? "mouse:" + std::string(n) : std::string();
    }
    case SourceKind::MouseAxis: {
        const char* n = reverse(MOUSE_AXES, src.code);
        return n != nullptr ? "mouseaxis:" + sign + n : std::string();
    }
    case SourceKind::PadButton: {
        const char* n = reverse(PAD_BUTTONS, src.code);
        return n != nullptr ? "pad:" + std::string(n) : std::string();
    }
    case SourceKind::PadAxis: {
        const char* n = reverse(PAD_AXES, src.code);
        if (n == nullptr) return std::string();
        const bool is_trigger = src.code == ::input::code::LT || src.code == ::input::code::RT;
        return (is_trigger ? "trigger:" : "padaxis:") + sign + n;
    }
    default:
        return std::string();
    }
}

} // namespace framework::input
