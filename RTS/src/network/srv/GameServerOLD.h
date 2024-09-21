#pragma once

#include "network/GameConnectionConfig.h"
#include "network/Message.h"

class SrvAdapterOLD;
class World;

struct PingMessage;

extern void logSrv(const wchar_t* str);
extern void logSrv(const std::string& str);

// Networking resources:
// https://isetta.io/compendium/Networking/
// https://www.gamedevs.org/uploads/tribes-networking-model.pdf
// http://mrelusive.com/publications/papers/The-DOOM-III-Network-Architecture.pdf
// https://www.amazon.com/Multiplayer-Game-Programming-Architecting-Networked/dp/0134034309/ref=sr_1_1?crid=2DRHA7SW8Y1BD&keywords=multiplayer+game+programming&qid=1663008452&s=books&sprefix=multiplayer+game+programming%2Cstripbooks%2C126&sr=1-1&ufe=app_do%3Aamzn1.fos.18ed3cb5-28d5-4975-8bc7-93deae8f9840

enum class ClientFlags : ui8 {
    JOINED = 1 << 0,
};

typedef std::vector<int> ClientList;
typedef std::vector<BitFlags<ClientFlags>> ClientFlagsList;

class GameServerOLD {
protected:
    GameServerOLD(World& world, ServerType serverType);
    ~GameServerOLD();

public:
    GameServerOLD(GameServerOLD& other) = delete;
    void operator=(const GameServerOLD&) = delete;

    static GameServerOLD& initInstance(World& world, ServerType serverType);
    static GameServerOLD& getInstance();
    static void destroyInstance();
    static bool exists() { return sInstance != nullptr; }

    void start();
    int tryTick();
    void stop();

    void clientConnected(int clientIndex);
    void clientDisconnected(int clientIndex);
    void shutdown() { mRunning = false; }

    // Messaging
    MessageBase* createMessage(int clientIndex, int type) { return (MessageBase*)mServer.CreateMessage(clientIndex, type); }
    void sendMessage(int clientIndex, MessageBase* message) { mServer.SendMessage(clientIndex, e_cast(MESSAGE_CHANNELS[message->GetType()]), message); }

    bool isRunning() const { return mRunning; }
    const yojimbo::Address& getServerAddress() const { return mServerAddress; }

    const ClientList& getClients() const { return mConnectedClients; }
private:
    void update();
    void updateConnectedClientBits();
    void processMessages();
    void processMessage(int clientIndex, yojimbo::Message* message);

    // TODO: SrvMessage?
    void processPingMessage(int clientIndex, PingMessage* message);
    void processClientReadyJoinMessage(int clientIndex);
    void processClientPlayerStateMessage(int clientIndex, ClientPlayerStateMessage* message);

    yojimbo::Address initServerAddress(ServerType serverType);

    void onClientConnected(int clientIndex);
    void onClientDisconnected(int clientIndex);

    void replicateStartGameStateToClient(int clientIndex);
    void replicateEntities();

    // MAINTAIN ORDER
    GameConnectionConfig mConnectionConfig;
    yojimbo::Address mServerAddress;
    std::unique_ptr<SrvAdapterOLD> mAdapter;
    yojimbo::Server mServer;
    ClientBits mConnectedClientBits = 0;
    ClientList mConnectedClients;
    ClientFlagsList mConnectedClientFlags;
    // MAINTAIN ORDER

    World& mWorld;
    ServerType mServerType;
    std::atomic_bool mRunning;
    double mTimeSec;
    bool mWantsStart = false;
    TickingTimer mTickTimer = TickingTimer((1.0f / SERVER_TICK_RATE_HZ) * MS_PER_SECOND, 64.0f);

    // TODO: SrvPlayerManager
    std::vector<entt::entity> mClientPlayerEntities;

    static GameServerOLD* sInstance;
};