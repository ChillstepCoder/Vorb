#include "stdafx.h"
#include "GameClient.h"

#include "network/cli/CliAdapter.h"
// TODO: use Yojimbo::NetworkSimulator

GameClient::GameClient(ClientConnectionType connectionType) : mAdapter(std::make_unique<CliAdapter>()), mConnectionType(connectionType) {

    switch (mConnectionType) {
        case ClientConnectionType::STANDALONE:
            break;
        case ClientConnectionType::LAN:
            break;
        case ClientConnectionType::DEDICATED_SERVER:
            mClient = std::make_unique<yojimbo::Client>(yojimbo::GetDefaultAllocator(), yojimbo::Address("0.0.0.0"), mConnectionConfig, *mAdapter, 0.0);
            break;
        default:
            assert(false);
    }
}

GameClient::~GameClient() {

}

void GameClient::connect(const uint8_t privateKey[], const yojimbo::Address& address) {

    switch (mConnectionType) {
        case ClientConnectionType::STANDALONE: {
            break;
        }
        case ClientConnectionType::LAN: {
            break;
        }
        case ClientConnectionType::DEDICATED_SERVER: {
            // TODO: Client ID should come from a backend
            uint64_t clientId;
            yojimbo::random_bytes((uint8_t*)&clientId, 8);
            // TODO: Secure connect
            ((yojimbo::Client*)mClient.get())->InsecureConnect(privateKey, clientId, address);
            break;
        }
        default:
            assert(false);
    }
}

void GameClient::disconnect() {
    mClient->Disconnect();
}

void GameClient::update(double dt) {

    mClient->AdvanceTime(mClient->GetTime() + dt);
    mClient->ReceivePackets();

    if (mClient->IsConnected()) {
        processMessages();

    }

    mClient->SendPackets();
}

void GameClient::processMessages() {

    for (int i = 0; i < mConnectionConfig.numChannels; i++) {
        yojimbo::Message* message = mClient->ReceiveMessage(i);
        while (message != NULL) {
            //ProcessMessage(message);
            mClient->ReleaseMessage(message);
            message = mClient->ReceiveMessage(i);
        }
    }
}
