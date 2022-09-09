#pragma once

#include <yojimbo/yojimbo.h>

// TODO: Real private key!
constexpr uint8_t DEFAULT_PRIVATE_KEY[yojimbo::KeyBytes] = { 0 };

enum class ClientConnectionType {
    INVALID,
    STANDALONE,
    LAN,
    DEDICATED_SERVER,
};

enum class GameChannel : ui8 {
    RELIABLE,
    UNRELIABLE,
    COUNT
};
