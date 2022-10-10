#include "stdafx.h"
#include "SrvMessage.h"

#include "network/srv/GameServer.h"

void SrvMessage::sendEntityCreateMessage(entt::entity srvEntity, const f32v3& startPos, f32 rotation)
{
    GameServer& server = GameServer::getInstance();
    EntityCreateMessage* message = server.CreateMessage(clientIndex, e_cast(MessageTypes::CLIENT_BEGIN));
    mServer.SendMessage(clientIndex, e_cast(MESSAGE_CHANNELS[message->GetType()]), beginMessage);
}
