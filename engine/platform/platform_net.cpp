#include "platform_net.hpp"

#include <cstdio>

// Общая половина шва: разбор и печать адреса. Сокетов здесь нет вовсе, поэтому файл собирается
// на всех трёх ОС один и тот же.
namespace platform {
namespace net {
namespace {

// Своя цифра вместо strtoul: тот принимает ведущие пробелы, знак и шестнадцатеричные формы, а
// "1.2.3.+4" и " 1.2.3.4" обязаны быть отвергнуты. Ограничение сверху — не защита от
// переполнения, а часть контракта: октет 256 и порт 65536 отличаются от валидных только
// значением, и молча свернуть их по модулю значило бы принять чужой адрес за свой.
//
// Ведущий ноль при длине больше цифры — тоже отказ, и это не педантизм о форме. inet_aton и
// getaddrinfo читают такую группу как ВОСЬМЕРИЧНУЮ, то есть "0177.0.0.1" у них 127.0.0.1, а у
// десятичного разбора 177.0.0.1. Принять её значило бы завести вторую запись одного адреса, по
// которой наш фильтр и системный резолвер расходятся, — ровно то, чем обходят списки доверия.
bool take_number(const std::string& s, size_t& i, uint32_t limit, uint32_t* out) {
    if (i >= s.size() || s[i] < '0' || s[i] > '9') return false;
    const bool leading_zero = s[i] == '0';
    uint32_t v = 0;
    size_t digits = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + static_cast<uint32_t>(s[i] - '0');
        ++i;
        ++digits;
        if (v > limit || digits > 5) return false;
    }
    if (leading_zero && digits > 1) return false;
    *out = v;
    return true;
}

} // namespace

bool parse_endpoint(const std::string& text, Endpoint* out) {
    if (out == nullptr) return false;
    size_t i = 0;
    uint32_t octets[4] = {0, 0, 0, 0};
    for (int k = 0; k < 4; ++k) {
        if (!take_number(text, i, 255, &octets[k])) return false;
        const char want = (k < 3) ? '.' : ':';
        if (i >= text.size() || text[i] != want) return false;
        ++i;
    }
    uint32_t port = 0;
    if (!take_number(text, i, 65535, &port)) return false;
    // Хвост после порта — отказ, а не «разобрали сколько смогли»: "127.0.0.1:80x" не адрес.
    if (i != text.size() || port == 0) return false;
    out->address = (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
    out->port = static_cast<uint16_t>(port);
    return true;
}

std::string endpoint_text(const Endpoint& e) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u", (e.address >> 24) & 0xFFu,
                  (e.address >> 16) & 0xFFu, (e.address >> 8) & 0xFFu, e.address & 0xFFu, e.port);
    return std::string(buf);
}

} // namespace net
} // namespace platform
