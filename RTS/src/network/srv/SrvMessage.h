#pragma once

#include "network/Message.h"

// Static class that sends messages
class SrvMessage
{
public:
    static void sendEntityTransformMessage(int clientIndex, entt::entity srvEntity, const f32v3& pos, f32 rotation);
    static void sendEntityCreateMessage(int clientIndex, entt::entity srvEntity, StrToken entityToken, const f32v3& startPos, f32 rotation);
    static void sendEntityCreateMessageToAll(entt::entity srvEntity, StrToken entityToken, const f32v3& startPos, f32 rotation);
    static void sendEntityDestroyMessageToAll(entt::entity srvEntity);
    static void sendClientBeginMessageToAll(int playerClientIndex, entt::entity srvPlayerEntity, const f32v3& startPos, f32 rotation);
    static void sendCharacterStateMessage(int clientIndex, entt::entity srvEntity, const f32v3& pos, const f32v3& velocity, const f32v2& controlDirection, ui32 mDesiredLocomotionMode);
};

