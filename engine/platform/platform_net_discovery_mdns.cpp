#include "platform_net_discovery_backend.hpp"
#include "platform_net_mdns_wire.hpp"

#include <algorithm>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace platform::net {
namespace {

constexpr uint32_t TTL = 120;
constexpr uint64_t QUERY_MS = 1'500;

uint32_t outbound_ipv4() {
    const auto fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#if defined(_WIN32)
    if (fd == INVALID_SOCKET) return ADDRESS_ANY;
    int size = sizeof(sockaddr_in);
#else
    if (fd < 0) return ADDRESS_ANY;
    socklen_t size = sizeof(sockaddr_in);
#endif
    sockaddr_in group{};
    group.sin_family = AF_INET;
    group.sin_port = htons(mdns::MULTICAST.port);
    group.sin_addr.s_addr = htonl(mdns::MULTICAST.address);
    sockaddr_in local{};
    const bool ok = ::connect(fd, reinterpret_cast<const sockaddr*>(&group), size) == 0 &&
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&local), &size) == 0;
#if defined(_WIN32)
    ::closesocket(fd);
#else
    ::close(fd);
#endif
    return ok ? ntohl(local.sin_addr.s_addr) : ADDRESS_ANY;
}

std::string hostname() {
    std::array<uint8_t, 16> random{};
    if (!secure_random(random)) return {};
    constexpr char HEX[] = "0123456789abcdef";
    std::string host = "likenes-";
    for (size_t i = 0; i < 8; ++i) {
        host.push_back(HEX[random[i] >> 4]);
        host.push_back(HEX[random[i] & 15]);
    }
    return host + ".local";
}

class Mdns final : public DiscoveryBackend {
public:
    ~Mdns() override { if (local_.port) send(mdns::announce(instance_, host_, local_, 0)); }
    bool open(const DiscoveryConfig& config) override;
    void poll(uint64_t now) override;

private:
    struct Cache { mdns::Record record; uint64_t until = 0; };
    void send(const mdns::Packet& p) { if (p.size) socket_.send_to(mdns::MULTICAST, p.bytes.data(), p.size); }
    void ingest(const mdns::Message& packet, uint64_t now);
    void remember(const mdns::Record& r, uint64_t now);
    const Cache* lookup(std::string_view name, uint16_t type) const;
    void publish();
    void browse();

    Socket socket_;
    Endpoint local_;
    std::string instance_;
    std::string host_;
    std::array<Cache, 48> cache_{};
    uint64_t next_ = 0;
    uint64_t reply_after_ = 0;
};

bool Mdns::open(const DiscoveryConfig& config) {
    const uint32_t iface = config.local_only ? ADDRESS_LOOPBACK : ADDRESS_ANY;
    if (!socket_.open_multicast(mdns::MULTICAST.address, mdns::MULTICAST.port, iface)) return false;
    if (!config.port) return true;
    local_.address = iface ? iface : outbound_ipv4();
    host_ = hostname();
    if (!local_.address || host_.empty() || config.name.find('.') != std::string::npos) return false;
    instance_ = config.name + "." + mdns::SERVICE;
    for (char& c : instance_) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    local_.port = config.port;
    return true;
}

void Mdns::remember(const mdns::Record& r, uint64_t now) {
    Cache* free = nullptr;
    for (auto& entry : cache_) {
        const auto& old = entry.record;
        if (!entry.until && !free) free = &entry;
        if (!entry.until || old.type != r.type || old.name != r.name) continue;
        if (r.type == mdns::PTR && r.target != old.target) continue;
        if ((r.type == mdns::A || r.type == mdns::AAAA) && r.address != old.address) continue;
        free = &entry;
        break;
    }
    if (free) *free = r.ttl ? Cache{r, now + uint64_t{std::min(r.ttl, TTL)} * 1000} : Cache{};
}

const Mdns::Cache* Mdns::lookup(std::string_view name, uint16_t type) const {
    for (const auto& c : cache_) if (c.until && c.record.type == type && c.record.name == name) return &c;
    return nullptr;
}

void Mdns::ingest(const mdns::Message& packet, uint64_t now) {
    const auto records = std::span(packet.records).first(packet.count);
    for (const auto& r : records) {
        if ((r.type == mdns::PTR && r.name == mdns::SERVICE && !mdns::service_instance(r.target).empty()) ||
            (r.type == mdns::SRV && !mdns::service_instance(r.name).empty() && r.port && !r.target.empty())) {
            remember(r, now);
        }
    }
    for (const auto& r : records) {
        if (r.type != mdns::A && r.type != mdns::AAAA) continue;
        for (const auto& srv : cache_) {
            if (srv.until && srv.record.type == mdns::SRV && srv.record.target == r.name) {
                remember(r, now);
                break;
            }
        }
    }
}

void Mdns::publish() {
    clear();
    for (const auto& ptr : cache_) {
        if (!ptr.until || ptr.record.type != mdns::PTR) continue;
        const auto* srv = lookup(ptr.record.target, mdns::SRV);
        if (!srv) continue;
        for (const auto& c : cache_) {
            if (!c.until || c.record.name != srv->record.target) continue;
            Endpoint e = c.record.address;
            if (c.record.type == mdns::A ? !e.address || (e.address >> 24) >= 224 :
                c.record.type != mdns::AAAA || e.address_v6 == Endpoint{}.address_v6 || e.address_v6[0] == 0xff ||
                (e.address_v6[0] == 0xfe && (e.address_v6[1] & 0xc0) == 0x80)) continue;
            e.port = srv->record.port;
            add(mdns::service_instance(ptr.record.target), e, 0);
        }
    }
}

void Mdns::browse() {
    send(mdns::query(mdns::SERVICE, mdns::PTR));
    for (const auto& ptr : cache_) {
        if (!ptr.until || ptr.record.type != mdns::PTR) continue;
        const auto* srv = lookup(ptr.record.target, mdns::SRV);
        if (!srv) { send(mdns::query(ptr.record.target, mdns::SRV)); break; }
        if (!lookup(srv->record.target, mdns::A) && !lookup(srv->record.target, mdns::AAAA)) {
            send(mdns::query(srv->record.target, mdns::A));
            send(mdns::query(srv->record.target, mdns::AAAA));
            break;
        }
    }
}

void Mdns::poll(uint64_t now) {
    for (auto& c : cache_) if (c.until && now >= c.until) c = {};
    bool changed = now >= next_;
    if (changed) {
        if (local_.port) send(mdns::announce(instance_, host_, local_, TTL));
        else browse();
        next_ = now + (local_.port ? 5'000 : QUERY_MS);
    }
    for (int i = 0; i < 16; ++i) {
        uint8_t buffer[MAX_DATAGRAM];
        Endpoint from;
        const int n = socket_.recv_from(buffer, sizeof(buffer), &from);
        if (n <= 0) { if (n < 0) good_ = false; break; }
        mdns::Message packet;
        if (from.port != mdns::MULTICAST.port || !mdns::parse({buffer, static_cast<size_t>(n)}, packet)) continue;
        if (!local_.port && packet.response) { ingest(packet, now); changed = true; }
        if (local_.port && !packet.response && now >= reply_after_) {
            for (const auto& q : std::span(packet.records).first(packet.count)) {
                if (q.name != mdns::SERVICE && q.name != instance_ && q.name != host_) continue;
                send(mdns::announce(instance_, host_, local_, TTL));
                reply_after_ = now + 100;
                break;
            }
        }
    }
    if (changed && !local_.port) publish();
}

} // namespace

std::unique_ptr<DiscoveryBackend> make_discovery_backend() { return std::make_unique<Mdns>(); }

} // namespace platform::net
