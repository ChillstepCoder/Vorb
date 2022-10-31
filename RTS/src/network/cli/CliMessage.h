#pragma once

#include "network/Message.h"

// Static class that sends messages
class CliMessage
{
public:
    static void sendClientReadyJoinMessage();
    static void sendPlayerStateMessage(const f32v3& pos, const f32v3& velocity, f32 controlAngle, ui32 mDesiredLocomotionMode);
};