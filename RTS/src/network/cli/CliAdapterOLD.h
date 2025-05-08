#pragma once

#include "network/NetworkConst.h"
#include "network/Message.h"

// Network adapter for the game server
class CliAdapterOLD : public yojimbo::Adapter
{
public:
    explicit CliAdapterOLD() {}

    yojimbo::MessageFactory* CreateMessageFactory(yojimbo::Allocator& allocator) override {
        return YOJIMBO_NEW(allocator, GameMessageFactory, allocator);
    }

    void OnServerClientConnected(int clientIndex) override;

    void OnServerClientDisconnected(int clientIndex) override;

};

