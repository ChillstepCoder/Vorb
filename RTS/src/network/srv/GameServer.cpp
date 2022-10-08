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

constexpr ui32 MAX_PLAYERS = 16;
constexpr int MAX_TICKS_IN_FRAME = 2;
constexpr f32 SERVER_BACKLOG_FASTFORWARD_TIME_SEC = 0.6; // Time differential before we just fast forward to make up for it

GameServer* GameServer::sInstance = nullptr;

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

}

GameServer& GameServer::initInstance(ServerType serverType) {

    if (!sHasInitYojimbo) {
        sHasInitYojimbo = true;
        InitializeYojimbo();
    }

    assert(!sInstance);
    sInstance = new GameServer(serverType);
    return *sInstance;
}

GameServer& GameServer::getInstance() {
    return *sInstance;
}

void GameServer::destroyInstance()
{
    assert(sInstance);
    delete sInstance;
    sInstance = nullptr;
}

GameServer::~GameServer() {
    mServer.Stop();
}

void GameServer::start() {

    // Loop
    mWantsStart = true;
    mRunning = true;

}

int GameServer::tryTick() {

    // We delay start until the first valid tick to avoid loading backlogs
    if (mWantsStart) {
        mWantsStart = false;
        mTimeSec = yojimbo_time();
    }

    constexpr float fixedDtSec = 1.0f / SERVER_TICK_RATE_HZ;
    mTickTimer.startFrame();
    int tickCount = 0;
    while (mTickTimer.tryTick()) {
        update();
        mTimeSec += fixedDtSec;
    }
    double currentTime = yojimbo_time();
    if (currentTime - mTimeSec >= SERVER_BACKLOG_FASTFORWARD_TIME_SEC) {
        std::cout << "Massive server time backlog detected! ";
        std::cout << mTimeSec << " " << currentTime << std::endl;
        mTimeSec = currentTime; // Fast forward
        return 1;
    }
    return 0;
}

void GameServer::stop() {
    mServer.Stop();
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
    mServer.AdvanceTime(mTimeSec);
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
        return NetworkUtil::getExternalIP(DEFAULT_SERVER_PORT, true /*ipv6*/);
    }
}
