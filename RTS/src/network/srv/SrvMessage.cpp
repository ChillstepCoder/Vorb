#include "stdafx.h"
#include "SrvMessage.h"

#include "network/srv/GameServerOLD.h"

void SrvMessage::sendEntityTransformMessage(int clientIndex, entt::entity srvEntity, const f32v3& pos, f32 rotation) {

    GameServerOLD& server = GameServerOLD::getInstance();
    EntityTransformMessage* message = (EntityTransformMessage*)server.createMessage(clientIndex, e_cast(MessageTypes::ENTITY_TRANSFORM));
    message->mSrvEntityID = (ui32)srvEntity;
    message->mPosition = pos;
    message->mRotation = rotation;
    server.sendMessage(clientIndex, message);
}

void SrvMessage::sendEntityCreateMessage(int clientIndex, entt::entity srvEntity, StrToken entityToken, const f32v3& startPos, f32 rotation) {

    GameServerOLD& server = GameServerOLD::getInstance();
    EntityCreateMessage* message = (EntityCreateMessage*)server.createMessage(clientIndex, e_cast(MessageTypes::ENTITY_CREATE));
    message->mSrvEntityID = (ui32)srvEntity;
    message->mPosition = startPos;
    message->mRotation = rotation;
    message->mEntityToken = entityToken;
    server.sendMessage(clientIndex, message);
}

void SrvMessage::sendEntityCreateMessageToAll(entt::entity srvEntity, StrToken entityToken, const f32v3& startPos, f32 rotation) {

    GameServerOLD& server = GameServerOLD::getInstance();
    const ClientList& clients = server.getClients();
    for (int clientIndex : clients) {
        sendEntityCreateMessage(clientIndex, srvEntity, entityToken, startPos, rotation);
    }
}

void SrvMessage::sendEntityDestroyMessageToAll(entt::entity srvEntity) {

    GameServerOLD& server = GameServerOLD::getInstance();
    const ClientList& clients = server.getClients();
    for (int clientIndex : clients) {
        GameServerOLD& server = GameServerOLD::getInstance();
        EntityDestroyMessage* message = (EntityDestroyMessage*)server.createMessage(clientIndex, e_cast(MessageTypes::ENTITY_DESTROY));
        message->mSrvEntityID = (ui32)srvEntity;
        server.sendMessage(clientIndex, message);
    }
}

void SrvMessage::sendClientBeginMessageToAll(int playerClientIndex, entt::entity srvPlayerEntity, const f32v3& startPos, f32 rotation) {

    GameServerOLD& server = GameServerOLD::getInstance();
    const ClientList& clients = server.getClients();
    for (int clientIndex : clients) {
        if (clientIndex == playerClientIndex) {
            // Notify the player that he is to be created, but only if this isn't the host
            if (clientIndex != CLIENT_INDEX_HOST) {
                ClientBeginMessage* message = (ClientBeginMessage*)server.createMessage(clientIndex, e_cast(MessageTypes::CLIENT_BEGIN));
                message->mSrvEntityID = (ui32)srvPlayerEntity;
                message->mPosition = startPos;
                message->mRotation = rotation;
                server.sendMessage(clientIndex, message);
            }
        }
        else {
            // Replicate the player entity to other clients
            EntityCreateMessage* message = (EntityCreateMessage*)server.createMessage(clientIndex, e_cast(MessageTypes::ENTITY_CREATE));
            message->mSrvEntityID = (ui32)srvPlayerEntity;
            message->mPosition = startPos;
            message->mRotation = rotation;
            message->mEntityToken = CStrToken("player");
            server.sendMessage(clientIndex, message);
        }
    }
}

void SrvMessage::sendCharacterStateMessage(int clientIndex, entt::entity srvEntity, const f32v3& pos, const f32v3& velocity, f32 controlAngle, ui32 mDesiredLocomotionMode) {

    GameServerOLD& server = GameServerOLD::getInstance();
    CharacterStateMessage* message = (CharacterStateMessage*)server.createMessage(clientIndex, e_cast(MessageTypes::CHARACTER_STATE));
    message->mSrvEntityID = (ui32)srvEntity;
    message->mPosition = pos;
    message->mControlAngle = controlAngle;
    message->mVelocity = velocity;
    message->mDesiredLocomotionMode = mDesiredLocomotionMode;
    server.sendMessage(clientIndex, message);
}
