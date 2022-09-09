#pragma once

#include "NetworkConst.h"

// the client and server config
struct GameConnectionConfig : yojimbo::ClientServerConfig {
    GameConnectionConfig() {
        numChannels = 2;
        channel[(int)GameChannel::RELIABLE].type = yojimbo::CHANNEL_TYPE_RELIABLE_ORDERED;
        channel[(int)GameChannel::UNRELIABLE].type = yojimbo::CHANNEL_TYPE_UNRELIABLE_UNORDERED;
    }
};