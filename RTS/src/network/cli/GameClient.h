#pragma once

#include "network/NetworkConst.h"
#include "network/GameConnectionConfig.h"
class CliAdapter;

namespace yojimbo {
    class BaseClient;
}

class GameClient
{
    GameClient(ClientConnectionType connectionType);
    ~GameClient();

private:
    GameConnectionConfig mConnectionConfig;
    std::unique_ptr<CliAdapter> mAdapter;
    std::unique_ptr<yojimbo::BaseClient> mClient = nullptr;
    ClientConnectionType mConnectionType = ClientConnectionType::INVALID;
};

