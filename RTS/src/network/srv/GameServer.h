#pragma once

#include "network/GameConnectionConfig.h"

constexpr int DEFAULT_SERVER_PORT = 45362;

class SrvAdapter;

struct PingMessage;

extern void logSrv(const wchar_t* str);
extern void logSrv(const std::string& str);

// Networking resources:
// https://isetta.io/compendium/Networking/
// https://www.gamedevs.org/uploads/tribes-networking-model.pdf
// http://mrelusive.com/publications/papers/The-DOOM-III-Network-Architecture.pdf
// https://www.amazon.com/Multiplayer-Game-Programming-Architecting-Networked/dp/0134034309/ref=sr_1_1?crid=2DRHA7SW8Y1BD&keywords=multiplayer+game+programming&qid=1663008452&s=books&sprefix=multiplayer+game+programming%2Cstripbooks%2C126&sr=1-1&ufe=app_do%3Aamzn1.fos.18ed3cb5-28d5-4975-8bc7-93deae8f9840


enum class ServerType {
    LAN,
    DEV,
    ONLINE
};

class GameServer {
public:
    GameServer(ServerType serverType);
    ~GameServer();

    int start();
    void clientConnected(int clientIndex);
    void clientDisconnected(int clientIndex);
    void shutdown() { mRunning = false; }

    bool isRunning() const { return mRunning; }
    const yojimbo::Address& getServerAddress() const { return mServerAddress; }

private:
    void update();
    void processMessages();
    void processMessage(int clientIndex, yojimbo::Message* message);

    // TODO: SrvMessage?
    void processPingMessage(int clientIndex, PingMessage* message);
    yojimbo::Address initServerAddress(ServerType serverType);

    // MAINTAIN ORDER
    GameConnectionConfig mConnectionConfig;
    yojimbo::Address mServerAddress;
    std::unique_ptr<SrvAdapter> mAdapter;
    yojimbo::Server mServer;
    // MAINTAIN ORDER

    ServerType mServerType;
    std::atomic_bool mRunning;
    double mTime;
};