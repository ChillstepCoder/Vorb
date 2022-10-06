#pragma once

#include <yojimbo/yojimbo.h>

// TODO: Real private key!
constexpr uint8_t DEFAULT_PRIVATE_KEY[yojimbo::KeyBytes] = { 0 };
constexpr int DEFAULT_SERVER_PORT = 45362;

enum class ClientConnectionType {
    INVALID,
    STANDALONE,
    LAN,
    ONLINE,
};

enum class GameChannel : ui8 {
    RELIABLE,
    UNRELIABLE,
    COUNT
};
