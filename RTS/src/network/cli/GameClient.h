#pragma once

#include "network/NetworkConst.h"
#include "network/GameConnectionConfig.h"
class CliAdapter;

namespace yojimbo {
    class BaseClient;
}

class GameClient
{
public:
    GameClient(ClientConnectionType connectionType);
    ~GameClient();

    void connect(const uint8_t privateKey[], const yojimbo::Address& address);
    void disconnect();
    void update(double dt);

    bool isConnected() const { return mClient->IsConnected(); }

private:
    void processMessages();

    GameConnectionConfig mConnectionConfig;
    std::unique_ptr<CliAdapter> mAdapter;
    std::unique_ptr<yojimbo::BaseClient> mClient = nullptr;
    ClientConnectionType mConnectionType = ClientConnectionType::INVALID;
};

