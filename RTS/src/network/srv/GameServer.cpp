#include "stdafx.h"

#include "GameServer.h"
#include "SrvAdapter.h"

#include "network/NetworkUtil.h"


// TODO fix these things, figure out unicode
void logSrv(const wchar_t* str) {
    //OutputDebugString(str);
    wprintf(str);
}

void logSrv(const std::string& str) {
    std::wstring wstr;
    wstr.assign(str.begin(), str.end());
    wprintf(wstr.data());
}

#define SERVER_TICK_RATE_HZ 64.0f

constexpr ui32 MAX_PLAYERS = 16;

GameServer::GameServer(ServerType serverType) :
    mAdapter(std::make_unique<SrvAdapter>(*this)),
    mServer(yojimbo::GetDefaultAllocator(), DEFAULT_PRIVATE_KEY, initServerAddress(serverType), mConnectionConfig, *mAdapter, 0.0),
    mServerType(serverType) {

    // start the server
    mServer.Start(MAX_PLAYERS);
    if (!mServer.IsRunning()) {
        char buffer[256];
        mServerAddress.ToString(buffer, sizeof(buffer));
        throw std::runtime_error("Could not start server " + std::string(buffer));
    }

    // Cache this after starting
    mServerAddress = mServer.GetAddress();

    // print the port we got in case we used port 0
    char buffer[256];
    mServerAddress.ToString(buffer, sizeof(buffer));
    printf("Server initializing with address %s\n", buffer);

    // ... load game ...

}

GameServer::~GameServer() {
    mServer.Stop();
}

int GameServer::start() {

    // Loop
    constexpr float fixedDt = 1.0f / SERVER_TICK_RATE_HZ;
    mTime = yojimbo_time();
    mRunning = true;

    while (mRunning) {
        double currentTime = yojimbo_time();
        if (mTime <= currentTime) {
            update();
            mTime += fixedDt;
        }
        else {
            yojimbo_sleep(mTime - currentTime);
        }
    }

    mServer.Stop();
    return 0;
}

void GameServer::clientConnected(int clientIndex) {
    char buffer[512];
    sprintf_s(buffer, "Client %d connected", clientIndex);
    logSrv(buffer);
}

void GameServer::clientDisconnected(int clientIndex) {
    char buffer[512];
    sprintf_s(buffer, "Client %d disconnected", clientIndex);
    logSrv(buffer);
}

void GameServer::update()
{
    // stop if server is not running
    if (!mServer.IsRunning()) {
        mRunning = false;
        return;
    }

    // update server and process messages
    mServer.AdvanceTime(mTime);
    mServer.ReceivePackets();
    processMessages();

    // ... process client inputs ...
    // ... update game ...
    // ... send game state to clients ...

    mServer.SendPackets();
}

void GameServer::processMessages() {
    for (int clientIndex = 0; clientIndex < MAX_PLAYERS; ++clientIndex) {
        if (mServer.IsClientConnected(clientIndex)) {
            for (int channelIndex = 0; channelIndex < mConnectionConfig.numChannels; ++channelIndex) {
                yojimbo::Message* message = mServer.ReceiveMessage(clientIndex, channelIndex);
                while (message != nullptr) {
                    processMessage(clientIndex, message);
                    mServer.ReleaseMessage(clientIndex, message);
                    message = mServer.ReceiveMessage(clientIndex, channelIndex);
                }
            }
        }
    }
}

void GameServer::processMessage(int clientIndex, yojimbo::Message* message) {
    switch (message->GetType()) {
        case (int)MessageTypes::PING:
            processPingMessage(clientIndex, (PingMessage*)message);
            break;
        default:
            break;
    }
}

void GameServer::processPingMessage(int clientIndex, PingMessage* message) {
    // Reply with same message so client can compute ping
    // TODO: Server also compute ping?
    PingMessage* pingMessage = (PingMessage*)mServer.CreateMessage(clientIndex, e_cast(MessageTypes::PING));
    pingMessage->mTimeStamp = message->mTimeStamp;
    mServer.SendMessage(clientIndex, e_cast(MESSAGE_CHANNELS[message->GetType()]), pingMessage);
}

yojimbo::Address GameServer::initServerAddress(ServerType serverType)
{
    if (serverType == ServerType::DEV) {
        return yojimbo::Address("127.0.0.1", DEFAULT_SERVER_PORT);
    }
    else if (serverType == ServerType::LAN) {
        return yojimbo::Address(NetworkUtil::getLocalIP().c_str(), DEFAULT_SERVER_PORT);
    }
    else {
        return NetworkUtil::getExternalIP(DEFAULT_SERVER_PORT);
    }
}
