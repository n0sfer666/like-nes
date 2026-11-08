#include "platform_net_mdns_wire.hpp"

namespace platform::net::mdns {
namespace {

constexpr uint16_t IN = 1;
constexpr uint16_t CACHE_FLUSH = 0x8000u;

struct Reader {
    std::span<const uint8_t> bytes;
    size_t at = 0;

    bool u16(uint16_t& out) {
        if (bytes.size() - at < 2) return false;
        out = static_cast<uint16_t>((uint16_t{bytes[at]} << 8) | bytes[at + 1]);
        at += 2;
        return true;
    }
    bool u32(uint32_t& out) {
        uint16_t hi = 0, lo = 0;
        if (!u16(hi) || !u16(lo)) return false;
        out = (uint32_t{hi} << 16) | lo;
        return true;
    }

    bool name(std::string& out) {
        size_t cursor = at;
        bool jumped = false;
        std::string result;
        for (int i = 0; i < 128 && cursor < bytes.size(); ++i) {
            const uint8_t size = bytes[cursor++];
            if (!size) {
                if (!jumped) at = cursor;
                out = std::move(result);
                return true;
            }
            if ((size & 0xc0) == 0xc0) {
                if (cursor == bytes.size()) return false;
                const size_t target = (size_t{size & 0x3fu} << 8) | bytes[cursor++];
                if (target >= cursor - 2) return false;
                if (!jumped) at = cursor;
                jumped = true;
                cursor = target;
                continue;
            }
            if (size > 63 || size > bytes.size() - cursor || result.size() + size + 1 > 254) return false;
            if (!result.empty()) result.push_back('.');
            for (size_t j = 0; j < size; ++j) {
                auto c = bytes[cursor++];
                if (c == 0 || c == '.') return false;
                if (c >= 'A' && c <= 'Z') c = static_cast<uint8_t>(c + ('a' - 'A'));
                result.push_back(static_cast<char>(c));
            }
        }
        return false;
    }
};

struct Writer {
    Packet packet;
    bool ok = true;
    void u8(uint8_t v) {
        if (packet.size == packet.bytes.size()) { ok = false; return; }
        packet.bytes[packet.size++] = v;
    }
    void u16(uint16_t v) { u8(static_cast<uint8_t>(v >> 8)); u8(static_cast<uint8_t>(v)); }
    void u32(uint32_t v) { u16(static_cast<uint16_t>(v >> 16)); u16(static_cast<uint16_t>(v)); }
    void name(std::string_view value) {
        while (!value.empty()) {
            const auto dot = value.find('.');
            const auto label = value.substr(0, dot);
            if (label.empty() || label.size() > 63) { ok = false; return; }
            u8(static_cast<uint8_t>(label.size()));
            for (char c : label) u8(static_cast<uint8_t>(c));
            if (dot == std::string_view::npos) break;
            value.remove_prefix(dot + 1);
        }
        u8(0);
    }
    void header(uint16_t flags, uint16_t questions, uint16_t answers, uint16_t extra) {
        u16(0); u16(flags); u16(questions); u16(answers); u16(0); u16(extra);
    }
    void rr(std::string_view owner, uint16_t type, uint32_t ttl, uint16_t length) {
        name(owner);
        u16(type);
        u16(type == PTR ? IN : IN | CACHE_FLUSH);
        u32(ttl);
        u16(length);
    }
    Packet finish() { if (!ok) packet.size = 0; return packet; }
};

bool resource(Reader& r, Record& entry) {
    uint16_t cls = 0, len = 0;
    if (!r.name(entry.name) || !r.u16(entry.type) || !r.u16(cls) || !r.u32(entry.ttl) ||
        !r.u16(len) || len > r.bytes.size() - r.at) return false;
    const auto end = r.at + len;
    if ((cls & ~CACHE_FLUSH) != IN) {
        entry.type = 0;
    } else if (entry.type == PTR) {
        if (!r.name(entry.target) || r.at != end) return false;
    } else if (entry.type == SRV) {
        uint16_t priority = 0, weight = 0;
        if (len < 7 || !r.u16(priority) || !r.u16(weight) || !r.u16(entry.port) ||
            !r.name(entry.target) || r.at != end) return false;
    } else if (entry.type == A) {
        if (len != 4 || !r.u32(entry.address.address)) return false;
    } else if (entry.type == AAAA) {
        if (len != 16) return false;
        entry.address.family = AddressFamily::IPv6;
        for (auto& octet : entry.address.address_v6) octet = r.bytes[r.at++];
    }
    r.at = end;
    return true;
}

} // namespace

bool parse(std::span<const uint8_t> bytes, Message& out) {
    Reader r{bytes};
    uint16_t id = 0, flags = 0, q = 0, a = 0, ns = 0, extra = 0;
    if (!r.u16(id) || !r.u16(flags) || !r.u16(q) || !r.u16(a) || !r.u16(ns) || !r.u16(extra) ||
        id != 0 || (flags & 0x780fu)) return false;
    const size_t count = size_t{q} + a + ns + extra;
    Message message;
    if (count > message.records.size()) return false;
    message.response = (flags & 0x8000u) != 0;
    for (uint16_t i = 0; i < q; ++i) {
        Record question;
        uint16_t cls = 0;
        if (!r.name(question.name) || !r.u16(question.type) || !r.u16(cls)) return false;
        if (!message.response && (cls & ~CACHE_FLUSH) == IN) message.records[message.count++] = std::move(question);
    }
    for (size_t i = q; i < count; ++i) {
        Record entry;
        if (!resource(r, entry)) return false;
        if (message.response && entry.type) message.records[message.count++] = std::move(entry);
    }
    if (r.at != bytes.size()) return false;
    out = std::move(message);
    return true;
}

Packet query(std::string_view name, uint16_t type) {
    Writer w;
    w.header(0, 1, 0, 0);
    w.name(name);
    w.u16(type);
    w.u16(IN);
    return w.finish();
}

Packet announce(std::string_view instance, std::string_view host, Endpoint address, uint32_t ttl) {
    if (!address.port || address.family != AddressFamily::IPv4 ||
        instance.size() > 253 || host.size() > 253) return {};
    Writer w;
    w.header(0x8400, 0, 1, 3);
    w.rr(SERVICE, PTR, ttl, static_cast<uint16_t>(instance.size() + 2));
    w.name(instance);
    w.rr(instance, SRV, ttl, static_cast<uint16_t>(host.size() + 8));
    w.u16(0); w.u16(0); w.u16(address.port);
    w.name(host);
    w.rr(instance, TXT, ttl, 1);
    w.u8(0);
    w.rr(host, A, ttl, 4);
    w.u32(address.address);
    return w.finish();
}

std::string_view service_instance(std::string_view name) {
    constexpr std::string_view suffix = "._likenes._udp.local";
    if (!name.ends_with(suffix)) return {};
    auto label = name.substr(0, name.size() - suffix.size());
    if (label.empty() || label.find('.') != std::string_view::npos) return {};
    return label;
}

} // namespace platform::net::mdns
