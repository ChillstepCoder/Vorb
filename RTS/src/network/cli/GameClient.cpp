#include "stdafx.h"
#include "GameClient.h"

#include "network/cli/CliAdapter.h"
// TODO: use Yojimbo::NetworkSimulator

GameClient::GameClient(ClientConnectionType connectionType) : mAdapter(std::make_unique<CliAdapter>()), mConnectionType(connectionType)
{
    switch (connectionType) {
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

GameClient::~GameClient()
{

}
