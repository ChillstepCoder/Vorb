#pragma once

#include "network/Message.h"

// Static class that sends messages
class SrvMessage
{
public:
    static void sendEntityCreateMessage(entt::entity srvEntity, const f32v3& startPos, f32 rotation);
};

