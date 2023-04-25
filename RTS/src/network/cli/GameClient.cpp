#include "stdafx.h"
#include "GameClient.h"

#include "world/IWorld.h"
#include "ecs/cli/CliEntityComponentSystem.h"
#include "network/NetworkUtil.h"

#include "network/cli/CliAdapter.h"
#include "network/cli/CliMessage.h"
// TODO: use Yojimbo::NetworkSimulator

constexpr f64 PING_INTERVAL_SEC = 0.1; // 100ms ping interval

GameClient* GameClient::sInstance = nullptr;

GameClient::GameClient(ServerType connectionType, const yojimbo::Address& hostAddress) : mAdapter(std::make_unique<CliAdapter>()), mConnectionType(connectionType), mHostAddress(hostAddress), mLastPingTimeS(yojimbo_time()) {

    switch (mConnectionType) {
        case ServerType::NONE:
            assert(false);
            break;
        case ServerType::LAN: {
            yojimbo::Address localAddress(NetworkUtil::getLocalIP().c_str(), DEFAULT_CLIENT_PORT);
            mClient = std::make_unique<yojimbo::Client>(yojimbo::GetDefaultAllocator(), localAddress, mConnectionConfig, *mAdapter, 0.0);
            break;
        }
        case ServerType::DEV: {
            mClient = std::make_unique<yojimbo::Client>(yojimbo::GetDefaultAllocator(), yojimbo::Address("0.0.0.0"), mConnectionConfig, *mAdapter, 0.0);
            break;
        }
        case ServerType::ONLINE: {
            yojimbo::Address externalAddress = NetworkUtil::getExternalIP(DEFAULT_CLIENT_PORT, hostAddress.GetType() == yojimbo::AddressType::ADDRESS_IPV6);
            mClient = std::make_unique<yojimbo::Client>(yojimbo::GetDefaultAllocator(), externalAddress, mConnectionConfig, *mAdapter, 0.0);
            break;
        }
        default:
            assert(false);
    }
}

GameClient& GameClient::initInstance(ServerType connectionType, const yojimbo::Address& hostAddress) {
    if (!sHasInitYojimbo) {
        sHasInitYojimbo = true;
        InitializeYojimbo();
    }

    assert(!sInstance);
    sInstance = new GameClient(connectionType, hostAddress);
    return* sInstance;
}

GameClient& GameClient::getInstance() {
    return *sInstance;
}

void GameClient::destroyInstance() {
    assert(sInstance);
    delete sInstance;
    sInstance = nullptr;
}

GameClient::~GameClient() {
    disconnect();
}

// To enable this we need to use a matcher service on a linux machine
#define USE_SECURE_CONNECT 0 

void GameClient::connect(const uint8_t privateKey[]) {

    switch (mConnectionType) {
        case ServerType::NONE: {
            assert(false);
            break;
        }
        case ServerType::DEV:
        case ServerType::LAN:
        case ServerType::ONLINE: {
            // TODO: Client ID should come from a backend
            uint64_t clientId;
            yojimbo::random_bytes((uint8_t*)&clientId, 8);

#if USE_SECURE_CONNECT == 1
            // See yojimbo::secure_client.cpp
            yojimbo::Matcher matcher(yojimbo::GetDefaultAllocator());

            if (!matcher.Initialize())
            {
                printf("error: failed to initialize network matcher\n");
                return;
            }

            matcher.RequestMatch(mConnectionConfig.protocolId, clientId, false);
            if (matcher.GetMatchStatus() == yojimbo::MATCH_FAILED)
            {
                printf("\nRequest match failed. Is the matcher running? Please run \"premake5 matcher\" before you connect a secure client\n");
                return;
            }

            uint8_t connectToken[yojimbo::ConnectTokenBytes];
            matcher.GetConnectToken(connectToken);

            ((yojimbo::Client*)mClient.get())->Connect(clientId, connectToken);//address);

#else
            ((yojimbo::Client*)mClient.get())->InsecureConnect(DEFAULT_PRIVATE_KEY, clientId, mHostAddress);
#endif
            break;
        }
        default:
            assert(false);
    }
}

void GameClient::disconnect() {
    mClient->Disconnect();
}

void GameClient::update(double dtSec) {

    mClient->AdvanceTime(mClient->GetTime() + dtSec);
    mClient->ReceivePackets();

    if (mClient->IsConnected()) {
        f64 mCurrentTime = yojimbo_time();
        if (mCurrentTime - mLastPingTimeS > PING_INTERVAL_SEC) {
            sendPingMessage(mCurrentTime);
        }

        processMessages();

        if (isJoined()) {
            replicatePlayerState();
        }
    }


    mClient->SendPackets();
}

void GameClient::processMessages() {

    for (int i = 0; i < mConnectionConfig.numChannels; i++) {
        yojimbo::Message* message = mClient->ReceiveMessage(i);
        while (message != NULL) {
            processMessage(message);
            mClient->ReleaseMessage(message);
            message = mClient->ReceiveMessage(i);
        }
    }
}

void GameClient::processMessage(yojimbo::Message* message) {

    switch (message->GetType()) {
        case (int)MessageTypes::PING:
            processPingMessage((PingMessage*)message);
            break;
        case (int)MessageTypes::CLIENT_BEGIN:
            processClientBeginMessage((ClientBeginMessage*)message);
            break;
        case (int)MessageTypes::ENTITY_CREATE:
            processEntityCreateMessage((EntityCreateMessage*)message);
            break;
        case (int)MessageTypes::ENTITY_TRANSFORM:
            processEntityTransformMessage((EntityTransformMessage*)message);
            break;
        case (int)MessageTypes::CHARACTER_STATE:
            processCharacterStateMessage((CharacterStateMessage*)message);
            break;
        default:
            break;
    }
}

void GameClient::sendPingMessage(f64 timestamp) {
    mLastPingTimeS = timestamp;
    PingMessage* pingMessage = (PingMessage*)mClient->CreateMessage(e_cast(MessageTypes::PING));
    pingMessage->mTimeStamp = timestamp;
    mClient->SendMessage(e_cast(MESSAGE_CHANNELS[pingMessage->GetType()]), pingMessage);
}

void GameClient::processPingMessage(PingMessage* message) {
    mCurrentPingMS = (f32)((yojimbo_time() - message->mTimeStamp) * MS_PER_SECOND_D);
}

void GameClient::processClientBeginMessage(ClientBeginMessage* message) {
    CliEntityComponentSystem& cliEcs = (CliEntityComponentSystem&)sMainGameWorld->getECS();
    assert(cliEcs.getLocalPlayer() == entt::null);
    entt::entity playerEntity = cliEcs.createEntityFromSrv((entt::entity)message->mSrvEntityID, message->mPosition, StrToken("player"));
    cliEcs.setLocalPlayer(playerEntity);
    mIsJoined = true;
}

void GameClient::processEntityCreateMessage(EntityCreateMessage* message) {
    CliEntityComponentSystem& cliEcs = (CliEntityComponentSystem&)sMainGameWorld->getECS();
    cliEcs.createEntityFromSrv((entt::entity)message->mSrvEntityID, message->mPosition, message->mEntityToken);
}

void GameClient::processEntityTransformMessage(EntityTransformMessage* message) {
    CliEntityComponentSystem& cliEcs = (CliEntityComponentSystem&)sMainGameWorld->getECS();
    entt::entity entity = cliEcs.getEntityFromSrvEntity((entt::entity)message->mSrvEntityID);
    if (entity != entt::null) {
        PhysicsComponent& physCmp = cliEcs.mRegistry.get<PhysicsComponent>(entity);
        physCmp.setTransform(message->mPosition, message->mRotation);
    }
}

void GameClient::processCharacterStateMessage(CharacterStateMessage* message) {
    CliEntityComponentSystem& cliEcs = (CliEntityComponentSystem&)sMainGameWorld->getECS();
    entt::entity entity = cliEcs.getEntityFromSrvEntity((entt::entity)message->mSrvEntityID);
    if (entity != entt::null) {
        PhysicsComponent& physCmp = cliEcs.mRegistry.get<PhysicsComponent>(entity);
        CharacterControlComponent& controlCmp = cliEcs.mRegistry.get<CharacterControlComponent>(entity);
        physCmp.setTransform(message->mPosition, 0.0f);
        physCmp.setVelocity(message->mVelocity);
        controlCmp.mControllerAngle = message->mControlAngle;
        controlCmp.mDesiredMode = (CharacterLocomotionMode)message->mDesiredLocomotionMode;
    }
}

void GameClient::replicatePlayerState() {
    CliEntityComponentSystem& cliEcs = (CliEntityComponentSystem&)sMainGameWorld->getECS();
    entt::entity entity = cliEcs.getLocalPlayer();
    if (entity != entt::null) {
        PhysicsComponent& physicsCmp = cliEcs.mRegistry.get<PhysicsComponent>(entity);
        CharacterControlComponent& controlCmp = cliEcs.mRegistry.get<CharacterControlComponent>(entity);
        CliMessage::sendPlayerStateMessage(physicsCmp.getPosition(), physicsCmp.getLinearVelocity(), controlCmp.mControllerAngle, e_cast(controlCmp.mDesiredMode));
    }
}
