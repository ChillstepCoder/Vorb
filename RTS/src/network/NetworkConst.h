#pragma once

#include <yojimbo/yojimbo.h>

// TODO: Real private key!
constexpr uint8_t DEFAULT_PRIVATE_KEY[yojimbo::KeyBytes] = { 0 };
constexpr int DEFAULT_SERVER_PORT = 45362;
constexpr int DEFAULT_CLIENT_PORT = 45361;

enum class ServerType {
    NONE,
    LAN,
    DEV,
    ONLINE
};

enum class GameChannel : ui8 {
    RELIABLE,
    UNRELIABLE,
    COUNT
};
