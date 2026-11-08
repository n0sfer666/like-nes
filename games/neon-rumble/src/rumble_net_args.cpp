#include "rumble_net_args.hpp"
#include "rumble_net_headless.hpp"

#include <charconv>
#include <string_view>

#include "bundle_view.hpp"
#include "platform_io.hpp"

namespace rumble {
namespace {

namespace udp = platform::net;

template<class Int>
bool number(std::string_view text, Int& out, int base = 10) {
    const auto* end = text.data() + text.size();
    const auto r = std::from_chars(text.data(), end, out, base);
    return r.ec == std::errc{} && r.ptr == end && out != 0;
}

} // namespace

size_t resolve_relay_address(std::string_view text, std::span<udp::Endpoint> endpoints) {
    if (endpoints.empty()) return 0;
    if (udp::parse_endpoint(std::string(text), &endpoints.front())) return 1;
    const auto colon = text.find(':');
    if (colon == std::string_view::npos || colon == 0 || colon != text.rfind(':')) return 0;
    uint16_t port = 0;
    if (!number(text.substr(colon + 1), port)) return 0;
    return udp::resolve_endpoints(std::string(text.substr(0, colon)), port, endpoints);
}

bool network_command(const char* command) {
    const std::string_view mode(command);
    return mode == "--net-host" || mode == "--net-join" || mode == "--net-create" || mode == "--net-code";
}

bool network_bundle_hash(const std::string& path, uint64_t& hash) {
    platform::MappedFile file;
    asset::BundleView bundle;
    if (!file.open(path) || !bundle.open(file.data(), file.size(), false)) return false;
    hash = bundle.header().bundle_hash;
    return true;
}

bool parse_network_run(int argc, char** argv, NetworkRun& out) {
    if (argc < 5 || !network_command(argv[1])) return false;
    NetworkRun run;
    const std::string_view mode(argv[1]);
    run.host = mode == "--net-host" || mode == "--net-create";
    run.relay = mode == "--net-create" || mode == "--net-code";
    const int frames = mode == "--net-code" ? 4 : 3;
    if (argc < frames + 2 || argc > frames + 3 ||
        std::string_view(argv[frames]) != "--frames" || !number(argv[frames + 1], run.frames)) return false;
    if (argc == frames + 3) {
        if (!run.relay || std::string_view(argv[frames + 2]) != "--relay-only") return false;
        run.relay_only = true;
    }
    if (mode == "--net-code" && (!number(argv[3], run.code, 16) || run.code > net::MAX_ROOM_CODE)) return false;
    run.endpoint_count = run.relay ? resolve_relay_address(argv[2], run.endpoints) :
        static_cast<size_t>(udp::parse_endpoint(argv[2], &run.endpoints[0]));
    if (!run.endpoint_count) return false;
    out = run;
    return true;
}

} // namespace rumble
