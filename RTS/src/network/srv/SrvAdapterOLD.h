#pragma once

#include "network/NetworkConst.h"
#include "network/Message.h"

class GameServerOLD;

// Network adapter for the game server
class SrvAdapterOLD : public yojimbo::Adapter
{
public: 
    explicit SrvAdapterOLD(GameServerOLD& gameServer) : mGameServer(gameServer) {}

    yojimbo::MessageFactory* CreateMessageFactory(yojimbo::Allocator& allocator) override {
        return YOJIMBO_NEW(allocator, GameMessageFactory, allocator);
    }

    void OnServerClientConnected(int clientIndex) override;

    void OnServerClientDisconnected(int clientIndex) override;

private:
    GameServerOLD& mGameServer;
};

