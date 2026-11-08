#include "platform_net.hpp"

#include <cstdio>

namespace platform {
namespace net {

bool Socket::open(const Endpoint& endpoint) { return open_impl(endpoint, 0, 0); }

bool Socket::open_multicast(uint32_t group, uint16_t port, uint32_t interface_address) {
    if ((group >> 28) != 0xe || !port) return false;
    return open_impl(Endpoint{ADDRESS_ANY, port}, group, interface_address);
}
namespace detail {
bool parse_ipv6(const char* text, std::array<uint8_t, 16>& out);
std::string format_ipv6(const std::array<uint8_t, 16>& bytes);
size_t resolve_host(const std::string& host, uint16_t port, std::span<Endpoint> out);
}
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
        const auto digit = static_cast<uint32_t>(s[i] - '0');
        if ((leading_zero && digits != 0) || v > (limit - digit) / 10) return false;
        v = v * 10 + digit;
        ++i;
        ++digits;
    }
    *out = v;
    return true;
}

bool take_ipv4(const std::string& text, size_t& cursor, uint32_t& address) {
    address = 0;
    for (int group = 0; group < 4; ++group) {
        if (group != 0 && (cursor == text.size() || text[cursor++] != '.')) return false;
        uint32_t octet = 0;
        if (!take_number(text, cursor, 255, &octet)) return false;
        address = (address << 8) | octet;
    }
    return true;
}

bool parse_v6_endpoint(const std::string& text, Endpoint& out) {
    const size_t close = text.find(']');
    if (close == std::string::npos || close + 1 >= text.size() || text[close + 1] != ':') return false;
    size_t cursor = close + 2;
    uint32_t port = 0;
    if (!take_number(text, cursor, 65535, &port) || port == 0 || cursor != text.size()) return false;

    const size_t percent = text.find('%', 1);
    const size_t end = percent < close ? percent : close;
    if (end <= 1 || end > 46 || text.find('\0') != std::string::npos) return false;
    Endpoint candidate{0, static_cast<uint16_t>(port), AddressFamily::IPv6};
    if (percent < close) {
        cursor = percent + 1;
        if (!take_number(text, cursor, UINT32_MAX, &candidate.scope_id) ||
            cursor != close || candidate.scope_id == 0) return false;
    }
    // Apply the same IPv4 rules inside IPv6 before the native parser sees it.
    const size_t dot = text.find('.', 1);
    if (dot < end) {
        const size_t colon = text.rfind(':', dot);
        if (colon == std::string::npos) return false;
        cursor = colon + 1;
        uint32_t unused = 0;
        if (!take_ipv4(text, cursor, unused) || cursor != end) return false;
    }
    if (!detail::parse_ipv6(text.substr(1, end - 1).c_str(), candidate.address_v6)) return false;
    const Endpoint canonical = normalized_endpoint(candidate);
    if (canonical.family == AddressFamily::IPv4 && candidate.scope_id != 0) return false;
    out = canonical;
    return true;
}

} // namespace

Endpoint normalized_endpoint(const Endpoint& endpoint) {
    if (endpoint.family != AddressFamily::IPv6) return endpoint;
    const auto& bytes = endpoint.address_v6;
    for (size_t i = 0; i < 10; ++i)
        if (bytes[i] != 0) return endpoint;
    if (bytes[10] != 0xff || bytes[11] != 0xff) return endpoint;
    uint32_t address = 0;
    for (size_t i = 12; i < bytes.size(); ++i) address = (address << 8) | bytes[i];
    return Endpoint{address, endpoint.port};
}

bool parse_endpoint(const std::string& text, Endpoint* out) {
    if (!out || text.empty()) return false;
    if (text.front() == '[') return parse_v6_endpoint(text, *out);
    size_t i = 0;
    uint32_t address = 0;
    if (!take_ipv4(text, i, address) || i == text.size() || text[i++] != ':') return false;
    uint32_t port = 0;
    if (!take_number(text, i, 65535, &port)) return false;
    // Хвост после порта — отказ, а не «разобрали сколько смогли»: "127.0.0.1:80x" не адрес.
    if (i != text.size() || port == 0) return false;
    *out = Endpoint{address, static_cast<uint16_t>(port)};
    return true;
}

std::string endpoint_text(const Endpoint& endpoint) {
    const Endpoint e = normalized_endpoint(endpoint);
    if (e.family == AddressFamily::IPv6) {
        const auto address = detail::format_ipv6(e.address_v6);
        if (address.empty()) return {};
        std::string text = "[" + address;
        if (e.scope_id != 0) text += "%" + std::to_string(e.scope_id);
        return text + "]:" + std::to_string(e.port);
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u", (e.address >> 24) & 0xFFu,
                  (e.address >> 16) & 0xFFu, (e.address >> 8) & 0xFFu, e.address & 0xFFu, e.port);
    return std::string(buf);
}

size_t resolve_endpoints(const std::string& host, uint16_t port, std::span<Endpoint> out) {
    if (port == 0 || out.empty() || host.empty() || host.size() > 254) return 0;
    for (char c : host) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-') continue;
        return 0;
    }
    return detail::resolve_host(host, port, out);
}

} // namespace net
} // namespace platform
