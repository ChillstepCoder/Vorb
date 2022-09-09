#pragma once

#include "network/NetworkConst.h"
#include "network/Message.h"

class GameServer;

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

