#pragma once

#include <yojimbo/yojimbo.h>

// TODO: Real private key!
constexpr uint8_t DEFAULT_PRIVATE_KEY[yojimbo::KeyBytes] = { 0 };
constexpr int DEFAULT_SERVER_PORT = 45362;
constexpr int DEFAULT_CLIENT_PORT = 45361;
constexpr ui32 MAX_CLIENTS = 16;
typedef ui16 ClientBits;
static_assert(MAX_CLIENTS <= 16, "Make sure ClientBits can store max number of clients");

typedef i32 ClientIndex;
constexpr ClientIndex CLIENT_INDEX_HOST = INT32_MAX;

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
