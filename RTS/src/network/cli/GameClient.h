#pragma once

#include "network/NetworkConst.h"
#include "network/GameConnectionConfig.h"
#include "network/Message.h"

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

    // Messaging
    MessageBase* createMessage(int type) { return (MessageBase*)mClient->CreateMessage(type); }
    void sendMessage(MessageBase* message) { mClient->SendMessage(e_cast(MESSAGE_CHANNELS[message->GetType()]), message); }

    bool isConnected() const { return mClient->IsConnected(); }
    bool isJoined() const { return mIsJoined; }

    f32 getCurrentPingMS() const { return mCurrentPingMS; }
    const yojimbo::Address& getClientAddress() const { return ((yojimbo::Client*)mClient.get())->GetAddress(); }

private:
    void processMessages();
    void processMessage(yojimbo::Message* message);

    // TODO: CliMessage?
    void sendPingMessage(f64 timestamp);
    void processPingMessage(PingMessage* message);
    void processClientBeginMessage(ClientBeginMessage* message);
    void processEntityCreateMessage(EntityCreateMessage* message);
    void processEntityTransformMessage(EntityTransformMessage* message);
    void processCharacterStateMessage(CharacterStateMessage* message);

    void replicatePlayerState();

    GameConnectionConfig mConnectionConfig;
    std::unique_ptr<CliAdapter> mAdapter;
    std::unique_ptr<yojimbo::BaseClient> mClient = nullptr;
    ServerType mConnectionType = ServerType::NONE;
    yojimbo::Address mHostAddress;
    f64 mLastPingTimeS;
    f32 mCurrentPingMS = 666.0f; // Sentinal ping meaning we havent checked ping yet
    bool mIsJoined = false;

    static GameClient* sInstance;
};


