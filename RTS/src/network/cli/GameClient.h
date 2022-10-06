#pragma once

#include "network/NetworkConst.h"
#include "network/GameConnectionConfig.h"
class CliAdapter;
struct PingMessage;

namespace yojimbo {
    class BaseClient;
}

class GameClient
{
protected:
    GameClient(ClientConnectionType connectionType);
    ~GameClient();

public:
    GameClient(GameClient& other) = delete;
    void operator=(const GameClient&) = delete;

    static GameClient& initInstance(ClientConnectionType connectionType);
    static GameClient& getInstance();
    static void destroyInstance();

    void connect(const uint8_t privateKey[], const yojimbo::Address& address);
    void disconnect();
    void update(double dtSec);

    bool isConnected() const { return mClient->IsConnected(); }

    f32 getCurrentPingMS() const { return mCurrentPingMS; }
    const yojimbo::Address& getClientAddress() const { assert(mConnectionType == ClientConnectionType::ONLINE); return ((yojimbo::Client*)mClient.get())->GetAddress(); }

private:
    void processMessages();
    void processMessage(yojimbo::Message* message);

    // TODO: CliMessage?
    void sendPingMessage(f64 timestamp);
    void processPingMessage(PingMessage* message);

    GameConnectionConfig mConnectionConfig;
    std::unique_ptr<CliAdapter> mAdapter;
    std::unique_ptr<yojimbo::BaseClient> mClient = nullptr;
    ClientConnectionType mConnectionType = ClientConnectionType::INVALID;
    f64 mLastPingTimeS;
    f32 mCurrentPingMS = 666.0f; // Sentinal ping meaning we havent checked ping yet

    static GameClient* sInstance;
};


