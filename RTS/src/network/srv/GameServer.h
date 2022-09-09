#pragma once

#include "network/GameConnectionConfig.h"

constexpr int DEFAULT_SERVER_PORT = 6669;

class SrvAdapter;

struct TestMessage;

extern void logSrv(const wchar_t* str);
extern void logSrv(const std::string& str);

class GameServer {
public:
    GameServer(const yojimbo::Address& address);
    ~GameServer();

    int start();
    void clientConnected(int clientIndex);
    void clientDisconnected(int clientIndex);

private:
    void update();
    void processMessages();
    void processMessage(int clientIndex, yojimbo::Message* message);

    void processTestMessage(int clientIndex, TestMessage* message);

    GameConnectionConfig mConnectionConfig;
    std::unique_ptr<SrvAdapter> mAdapter;
    yojimbo::Server mServer;
    bool mRunning;
    double mTime;
};