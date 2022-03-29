#pragma once

#include <yojimbo/yojimbo.h>

#include "network/Message.h"

class GameServer;

YOJIMBO_MESSAGE_FACTORY_START(GameMessageFactory, (int)MessageTypes::COUNT);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::TEST, TestMessage);
YOJIMBO_MESSAGE_FACTORY_FINISH();

// Network adapter for the game server
class SrvAdapter : public yojimbo::Adapter
{
public: 
    explicit SrvAdapter(GameServer& gameServer) : mGameServer(gameServer) {}

    yojimbo::MessageFactory* CreateMessageFactory(yojimbo::Allocator& allocator) override {
        return YOJIMBO_NEW(allocator, GameMessageFactory, allocator);
    }

    void OnServerClientConnected(int clientIndex) override;

    void OnServerClientDisconnected(int clientIndex) override;

private:
    GameServer& mGameServer;
};

