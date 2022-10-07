#include "stdafx.h"
#include "GameClient.h"

#include "network/NetworkUtil.h"

#include "network/cli/CliAdapter.h"
// TODO: use Yojimbo::NetworkSimulator

constexpr f64 PING_INTERVAL_SEC = 0.1; // 100ms ping interval

GameClient* GameClient::sInstance = nullptr;

GameClient::GameClient(ClientConnectionType connectionType) : mAdapter(std::make_unique<CliAdapter>()), mConnectionType(connectionType), mLastPingTimeS(yojimbo_time()) {

    switch (mConnectionType) {
        case ClientConnectionType::STANDALONE:
            assert(false);
            break;
        case ClientConnectionType::LAN: {
            yojimbo::Address localAddress(NetworkUtil::getLocalIP().c_str(), DEFAULT_SERVER_PORT);
            mClient = std::make_unique<yojimbo::Client>(yojimbo::GetDefaultAllocator(), localAddress, mConnectionConfig, *mAdapter, 0.0);
            break;
        }
        case ClientConnectionType::ONLINE: {
            yojimbo::Address externalAddress = NetworkUtil::getExternalIP(0);
            mClient = std::make_unique<yojimbo::Client>(yojimbo::GetDefaultAllocator(), externalAddress, mConnectionConfig, *mAdapter, 0.0);
            break;
        }
        default:
            assert(false);
    }
}

GameClient& GameClient::initInstance(ClientConnectionType connectionType) {
    if (!sHasInitYojimbo) {
        sHasInitYojimbo = true;
        InitializeYojimbo();
    }

    assert(!sInstance);
    sInstance = new GameClient(connectionType);
    return* sInstance;
}

GameClient& GameClient::getInstance() {
    return *sInstance;
}

void GameClient::destroyInstance() {
    assert(sInstance);
    delete sInstance;
    sInstance = nullptr;
}

GameClient::~GameClient() {
    disconnect();
}

// To enable this we need to use a matcher service on a linux machine
#define USE_SECURE_CONNECT 0 

void GameClient::connect(const uint8_t privateKey[], const yojimbo::Address& address) {

    switch (mConnectionType) {
        case ClientConnectionType::STANDALONE: {
            assert(false);
            break;
        }
        case ClientConnectionType::LAN:
        case ClientConnectionType::ONLINE: {
            // TODO: Client ID should come from a backend
            uint64_t clientId;
            yojimbo::random_bytes((uint8_t*)&clientId, 8);

#if USE_SECURE_CONNECT == 1
            // See yojimbo::secure_client.cpp
            yojimbo::Matcher matcher(yojimbo::GetDefaultAllocator());

            if (!matcher.Initialize())
            {
                printf("error: failed to initialize network matcher\n");
                return;
            }

            matcher.RequestMatch(mConnectionConfig.protocolId, clientId, false);
            if (matcher.GetMatchStatus() == yojimbo::MATCH_FAILED)
            {
                printf("\nRequest match failed. Is the matcher running? Please run \"premake5 matcher\" before you connect a secure client\n");
                return;
            }

            uint8_t connectToken[yojimbo::ConnectTokenBytes];
            matcher.GetConnectToken(connectToken);

            ((yojimbo::Client*)mClient.get())->Connect(clientId, connectToken);//address);

#else
            ((yojimbo::Client*)mClient.get())->InsecureConnect(DEFAULT_PRIVATE_KEY, clientId, address);
#endif
            break;
        }
        default:
            assert(false);
    }
}

void GameClient::disconnect() {
    mClient->Disconnect();
}

void GameClient::update(double dtSec) {

    mClient->AdvanceTime(mClient->GetTime() + dtSec);
    mClient->ReceivePackets();

    if (mClient->IsConnected()) {
        f64 mCurrentTime = yojimbo_time();
        if (mCurrentTime - mLastPingTimeS > PING_INTERVAL_SEC) {
            sendPingMessage(mCurrentTime);
        }

        processMessages();

    }

    mClient->SendPackets();
}

void GameClient::processMessages() {

    for (int i = 0; i < mConnectionConfig.numChannels; i++) {
        yojimbo::Message* message = mClient->ReceiveMessage(i);
        while (message != NULL) {
            processMessage(message);
            mClient->ReleaseMessage(message);
            message = mClient->ReceiveMessage(i);
        }
    }
}

void GameClient::processMessage(yojimbo::Message* message)
{
    switch (message->GetType()) {
        case (int)MessageTypes::PING:
            processPingMessage((PingMessage*)message);
            break;
        default:
            break;
    }
}

void GameClient::sendPingMessage(f64 timestamp) {
    mLastPingTimeS = timestamp;
    PingMessage* pingMessage = (PingMessage*)mClient->CreateMessage(e_cast(MessageTypes::PING));
    pingMessage->mTimeStamp = timestamp;
    mClient->SendMessage(e_cast(MESSAGE_CHANNELS[pingMessage->GetType()]), pingMessage);
}

void GameClient::processPingMessage(PingMessage* message) {
    mCurrentPingMS = (f32)((yojimbo_time() - message->mTimeStamp) * MS_PER_SECOND_D);
}
