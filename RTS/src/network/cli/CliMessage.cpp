#include "stdafx.h"
#include "CliMessage.h"

#include "network/cli/GameClient.h"

void CliMessage::sendClientReadyJoinMessage() {
    GameClient& client = GameClient::getInstance();
    ClientReadyJoinMessage* joinMsg = (ClientReadyJoinMessage*)client.createMessage(e_cast(MessageTypes::CLIENT_READY_JOIN));
    client.sendMessage(joinMsg);
}

void CliMessage::sendPlayerStateMessage(const f32v3& pos, const f32v3& velocity, const f32v2& controlDirection, ui32 mDesiredLocomotionMode) {
    GameClient& client = GameClient::getInstance();
    ClientPlayerStateMessage* message = (ClientPlayerStateMessage*)client.createMessage(e_cast(MessageTypes::CLIENT_PLAYER_STATE));
    message->mPosition = pos;
    message->mControlDirection = controlDirection;
    message->mVelocity = velocity;
    message->mDesiredLocomotionMode = mDesiredLocomotionMode;
    client.sendMessage(message);
}
