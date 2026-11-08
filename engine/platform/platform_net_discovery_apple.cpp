#include "platform_net_discovery_backend.hpp"

#include <arpa/inet.h>
#include <dns_sd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>

#include "platform_net_native.hpp"

namespace platform::net {
namespace {

class DnsRef {
public:
    DnsRef() = default;
    ~DnsRef() { reset(); }
    DnsRef(const DnsRef&) = delete;
    DnsRef& operator=(const DnsRef&) = delete;

    DNSServiceRef* out() { return &ref_; }
    void reset() {
        if (ref_) DNSServiceRefDeallocate(ref_);
        ref_ = nullptr;
    }
    bool poll() {
        if (!ref_) return true;
        pollfd fd{DNSServiceRefSockFD(ref_), POLLIN, 0};
        const int n = ::poll(&fd, 1, 0);
        if (n <= 0) return true;
        return (fd.revents & POLLIN) && DNSServiceProcessResult(ref_) == kDNSServiceErr_NoError;
    }

private:
    DNSServiceRef ref_ = nullptr;
};

class Bonjour final : public DiscoveryBackend {
public:
    bool open(const DiscoveryConfig& config) override;
    void poll(uint64_t now) override;

private:
    struct Candidate {
        Bonjour* owner = nullptr;
        std::string name;
        uint32_t iface = 0;
        uint16_t port = 0;
        DnsRef resolve;
        DnsRef address;
        bool resolved = false;
        bool failed = false;
        void reset() {
            resolve.reset();
            address.reset();
            owner = nullptr;
            name.clear();
            resolved = failed = false;
        }
    };

    static void registered(DNSServiceRef, DNSServiceFlags, DNSServiceErrorType error,
                           const char*, const char*, const char*, void* data) {
        if (error != kDNSServiceErr_NoError) static_cast<Bonjour*>(data)->good_ = false;
    }
    static void browsed(DNSServiceRef, DNSServiceFlags flags, uint32_t iface, DNSServiceErrorType error,
                        const char* name, const char* type, const char* domain, void* data);
    static void resolved(DNSServiceRef, DNSServiceFlags, uint32_t iface, DNSServiceErrorType error,
                         const char*, const char* target, uint16_t port, uint16_t, const unsigned char*, void* data);
    static void addressed(DNSServiceRef, DNSServiceFlags flags, uint32_t iface, DNSServiceErrorType error,
                          const char*, const sockaddr* address, uint32_t, void* data);

    void found(DNSServiceFlags flags, uint32_t iface, const char* name, const char* type, const char* domain);
    DnsRef root_;
    std::array<Candidate, DISCOVERY_ROOMS> pending_{};
};

bool Bonjour::open(const DiscoveryConfig& config) {
    const uint32_t iface = config.local_only ? kDNSServiceInterfaceIndexLocalOnly : 0;
    if (!config.port) {
        return DNSServiceBrowse(root_.out(), 0, iface, DISCOVERY_SERVICE, "local.", browsed, this) ==
            kDNSServiceErr_NoError;
    }
    return DNSServiceRegister(root_.out(), 0, iface, config.name.c_str(), DISCOVERY_SERVICE,
                              "local.", config.local_only ? "localhost." : nullptr,
                              htons(config.port), 0, nullptr, registered, this) ==
        kDNSServiceErr_NoError;
}

void Bonjour::poll(uint64_t) {
    if (!good_) return;
    const bool polled = root_.poll();
    good_ = good_ && polled;
    if (!good_) return;
    for (auto& c : pending_) {
        if (!c.owner) continue;
        if (!c.resolve.poll() || !c.address.poll() || c.failed) {
            remove(c.name, c.iface);
            c.reset();
        } else if (c.resolved) {
            c.resolve.reset();
        }
    }
}

void Bonjour::browsed(DNSServiceRef, DNSServiceFlags flags, uint32_t iface, DNSServiceErrorType error,
                      const char* name, const char* type, const char* domain, void* data) {
    auto& self = *static_cast<Bonjour*>(data);
    if (error != kDNSServiceErr_NoError) {
        self.good_ = false;
        return;
    }
    self.found(flags, iface, name, type, domain);
}

void Bonjour::found(DNSServiceFlags flags, uint32_t iface, const char* name, const char* type, const char* domain) {
    Candidate* free = nullptr;
    for (auto& c : pending_) {
        if (!c.owner && !free) free = &c;
        if (!c.owner || c.iface != iface || c.name != name) continue;
        if (!(flags & kDNSServiceFlagsAdd)) {
            remove(c.name, c.iface);
            c.reset();
        }
        return;
    }
    if (!(flags & kDNSServiceFlagsAdd) || !free) return;
    free->owner = this;
    free->name = name;
    free->iface = iface;
    if (DNSServiceResolve(free->resolve.out(), 0, iface, name, type, domain, resolved, free) !=
        kDNSServiceErr_NoError) free->reset();
}

void Bonjour::resolved(DNSServiceRef, DNSServiceFlags, uint32_t iface, DNSServiceErrorType error,
                        const char*, const char* target, uint16_t port, uint16_t, const unsigned char*, void* data) {
    auto& c = *static_cast<Candidate*>(data);
    if (c.resolved || c.failed) return;
    c.port = ntohs(port);
    c.resolved = true;
    if (error != kDNSServiceErr_NoError || !c.port ||
        DNSServiceGetAddrInfo(c.address.out(), 0, iface, kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6,
                              target, addressed, &c) != kDNSServiceErr_NoError) c.failed = true;
}

void Bonjour::addressed(DNSServiceRef, DNSServiceFlags flags, uint32_t iface, DNSServiceErrorType error,
                         const char*, const sockaddr* address, uint32_t, void* data) {
    auto& c = *static_cast<Candidate*>(data);
    if (error != kDNSServiceErr_NoError || !address) {
        c.failed = true;
        return;
    }
    Endpoint endpoint;
    if (address->sa_family != AF_INET && address->sa_family != AF_INET6) return;
    if (!detail::from_sockaddr(address, address->sa_len, endpoint)) return;
    endpoint.port = c.port;
    if (endpoint.family == AddressFamily::IPv6 && IN6_IS_ADDR_LINKLOCAL(
            &reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr) && !endpoint.scope_id) endpoint.scope_id = iface;
    if (flags & kDNSServiceFlagsAdd) c.owner->add(c.name, endpoint, c.iface);
    else c.owner->remove(c.name, endpoint, c.iface);
}

} // namespace

std::unique_ptr<DiscoveryBackend> make_discovery_backend() { return std::make_unique<Bonjour>(); }

} // namespace platform::net
