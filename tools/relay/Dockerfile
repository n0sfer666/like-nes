FROM alpine:3.22.8 AS build
RUN apk add --no-cache build-base
WORKDIR /src
COPY engine/asset/hash.hpp engine/asset/
COPY engine/net/net_relay_wire.cpp engine/net/net_relay_wire.hpp engine/net/net_relay_endpoint.hpp engine/net/net_session_wire.cpp engine/net/net_session_wire.hpp engine/net/net_wire.hpp engine/net/net_link.hpp engine/net/net_ack_window.hpp engine/net/
COPY engine/platform/platform_net.hpp engine/platform/platform_net.cpp engine/platform/platform_net_native.hpp engine/platform/platform_net_multicast_native.hpp engine/platform/platform_net_posix.cpp engine/platform/platform_args.hpp engine/platform/platform_args.cpp engine/platform/platform_args_posix.cpp engine/platform/
COPY tools/relay/net_relay_server.hpp tools/relay/net_relay_server.cpp tools/relay/net_relay_admission.cpp tools/relay/relay_main.cpp tools/relay/
RUN c++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread \
    -Iengine/net -Iengine/platform -Iengine/asset \
    engine/net/net_relay_wire.cpp engine/net/net_session_wire.cpp \
    engine/platform/platform_net.cpp engine/platform/platform_net_posix.cpp \
    engine/platform/platform_args.cpp engine/platform/platform_args_posix.cpp \
    tools/relay/net_relay_server.cpp tools/relay/net_relay_admission.cpp tools/relay/relay_main.cpp \
    -o /like_nes_relay

FROM alpine:3.22.8
RUN apk add --no-cache libstdc++
COPY --from=build /like_nes_relay /usr/local/bin/like_nes_relay
USER 65532:65532
EXPOSE 7777/udp
ENTRYPOINT ["/usr/local/bin/like_nes_relay"]
