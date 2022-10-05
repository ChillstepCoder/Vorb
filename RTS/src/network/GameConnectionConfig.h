#pragma once

#include "NetworkConst.h"

#define DEBUG_DISABLE_TIMEOUT 1
#define NO_NETWORK_SIMULATOR 1

#define PROTOCOL_VERSION 0 // Increment this each protocol version to keep client and server in sync

namespace {
    std::atomic_bool sHasInitYojimbo = false;
};

// the client and server config
struct GameConnectionConfig : yojimbo::ClientServerConfig {
    GameConnectionConfig() {
        protocolId = PROTOCOL_VERSION;
        numChannels = 2;
        channel[(int)GameChannel::RELIABLE].type = yojimbo::CHANNEL_TYPE_RELIABLE_ORDERED;
        channel[(int)GameChannel::UNRELIABLE].type = yojimbo::CHANNEL_TYPE_UNRELIABLE_UNORDERED;
#if DEBUG_DISABLE_TIMEOUT == 1
        timeout = -1.0f;
#endif
#if NO_NETWORK_SIMULATOR == 1
        networkSimulator = false;
#endif
    }
};