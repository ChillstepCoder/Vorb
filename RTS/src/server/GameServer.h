#pragma once

#include <yojimbo/yojimbo.h>


constexpr int DEFAULT_SERVER_PORT = 6669;

class SrvAdapter;

struct TestMessage;

extern void logSrv(const wchar_t* str);
extern void logSrv(const char* str);

enum class GameChannel {
    RELIABLE,
    UNRELIABLE,
    COUNT
};

// the client and server config
struct GameConnectionConfig : yojimbo::ClientServerConfig {
    GameConnectionConfig() {
        numChannels = 2;
        channel[(int)GameChannel::RELIABLE].type = yojimbo::CHANNEL_TYPE_RELIABLE_ORDERED;
        channel[(int)GameChannel::UNRELIABLE].type = yojimbo::CHANNEL_TYPE_UNRELIABLE_UNORDERED;
    }
};

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