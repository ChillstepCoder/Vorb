#include "stdafx.h"
#include "CliMessageOLD.h"

#include "network/cli/GameClientOLD.h"

void CliMessageOLD::sendClientReadyJoinMessage() {
    GameClientOLD& client = GameClientOLD::getInstance();
    ClientReadyJoinMessage* joinMsg = (ClientReadyJoinMessage*)client.createMessage(e_cast(MessageTypes::CLIENT_READY_JOIN));
    client.sendMessage(joinMsg);
}

void CliMessageOLD::sendPlayerStateMessage(const f32v3& pos, const f32v3& velocity, f32 controlAngle, ui32 mDesiredLocomotionMode) {
    GameClientOLD& client = GameClientOLD::getInstance();
    ClientPlayerStateMessage* message = (ClientPlayerStateMessage*)client.createMessage(e_cast(MessageTypes::CLIENT_PLAYER_STATE));
    message->mPosition = pos;
    message->mControlAngle = controlAngle;
    message->mVelocity = velocity;
    message->mDesiredLocomotionMode = mDesiredLocomotionMode;
    client.sendMessage(message);
}
