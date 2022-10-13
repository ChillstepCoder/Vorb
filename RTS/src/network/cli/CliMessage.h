#pragma once

#include "network/Message.h"

// Static class that sends messages
class CliMessage
{
public:
    static void sendClientReadyJoinMessage();
    static void sendPlayerStateMessage(const f32v3& pos, const f32v3& velocity, const f32v2& controlDirection, ui32 mDesiredLocomotionMode);
};