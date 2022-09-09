#pragma once

#include <yojimbo/yojimbo.h>

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
