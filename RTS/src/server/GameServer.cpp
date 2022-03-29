#include "stdafx.h"

#include "GameServer.h"
#include "SrvAdapter.h"

void logSrv(const wchar_t* str) {
    OutputDebugString(str);
    wprintf(str);
}

void logSrv(const char* str) {
    wchar_t wstr[512];
    wsprintf(wstr, L"%S", str);
    OutputDebugString(wstr);
    wprintf(wstr);
}


// TODO: Real private key!
constexpr uint8_t DEFAULT_PRIVATE_KEY[yojimbo::KeyBytes] = { 0 };
constexpr ui32 MAX_PLAYERS = 16;

GameServer::GameServer(const yojimbo::Address& address) :
    mAdapter(std::make_unique<SrvAdapter>(*this)),
    mServer(yojimbo::GetDefaultAllocator(), DEFAULT_PRIVATE_KEY, address, mConnectionConfig, *mAdapter, 0.0) {

    // start the server
    mServer.Start(MAX_PLAYERS);
    if (!mServer.IsRunning()) {
        throw std::runtime_error("Could not start server at port " + std::to_string(address.GetPort()));
    }

    // print the port we got in case we used port 0
    char buffer[256];
    mServer.GetAddress().ToString(buffer, sizeof(buffer));
    wchar_t wbuffer[512];
    wsprintf(wbuffer, L"Server address is %S", buffer);
    logSrv(wbuffer);

    // ... load game ...

}

GameServer::~GameServer() {
    mServer.Stop();
}

int GameServer::start() {


    // Loop
    mRunning = true;
    float fixedDt = 1.0f / 60.0f;
    mTime = yojimbo_time();

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
    wchar_t buffer[512];
    wsprintf(buffer, L"Client %d connected", clientIndex);
    logSrv(buffer);
}

void GameServer::clientDisconnected(int clientIndex) {
    wchar_t buffer[512];
    wsprintf(buffer, L"Client %d disconnected", clientIndex);
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
        case (int)MessageTypes::TEST:
            processTestMessage(clientIndex, (TestMessage*)message);
            break;
        default:
            break;
    }
}

void GameServer::processTestMessage(int clientIndex, TestMessage* message) {
    wchar_t buffer[512];
    wsprintf(buffer, L"Received test message from client %d %f", clientIndex, message->mData);
    logSrv(buffer);
}