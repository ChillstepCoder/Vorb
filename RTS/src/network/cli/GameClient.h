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
    GameClient(ServerType connectionType, const yojimbo::Address& hostAddress);
    ~GameClient();

public:
    GameClient(GameClient& other) = delete;
    void operator=(const GameClient&) = delete;

    static GameClient& initInstance(ServerType connectionType, const yojimbo::Address& hostAddress);
    static GameClient& getInstance();
    static void destroyInstance();

    void connect(const uint8_t privateKey[]);
    void disconnect();
    void update(double dtSec);

    bool isConnected() const { return mClient->IsConnected(); }

    f32 getCurrentPingMS() const { return mCurrentPingMS; }
    const yojimbo::Address& getClientAddress() const { return ((yojimbo::Client*)mClient.get())->GetAddress(); }

private:
    void processMessages();
    void processMessage(yojimbo::Message* message);

    // TODO: CliMessage?
    void sendPingMessage(f64 timestamp);
    void processPingMessage(PingMessage* message);

    GameConnectionConfig mConnectionConfig;
    std::unique_ptr<CliAdapter> mAdapter;
    std::unique_ptr<yojimbo::BaseClient> mClient = nullptr;
    ServerType mConnectionType = ServerType::NONE;
    yojimbo::Address mHostAddress;
    f64 mLastPingTimeS;
    f32 mCurrentPingMS = 666.0f; // Sentinal ping meaning we havent checked ping yet

    static GameClient* sInstance;
};


