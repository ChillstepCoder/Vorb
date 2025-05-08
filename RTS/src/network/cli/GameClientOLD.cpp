#include "stdafx.h"
#include "GameClientOLD.h"

#include "world/World.h"
#include "ecs/cli/CliFullECS.h"
#include "network/NetworkUtil.h"

#include "network/cli/CliAdapterOLD.h"
#include "network/cli/CliMessageOLD.h"
// TODO: use Yojimbo::NetworkSimulator

constexpr f64 PING_INTERVAL_SEC = 0.1; // 100ms ping interval

GameClientOLD* GameClientOLD::sInstance = nullptr;

GameClientOLD::GameClientOLD(ServerType connectionType, const yojimbo::Address& hostAddress) : mAdapter(std::make_unique<CliAdapterOLD>()), mConnectionType(connectionType), mHostAddress(hostAddress), mLastPingTimeS(yojimbo_time()) {

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

GameClientOLD& GameClientOLD::initInstance(ServerType connectionType, const yojimbo::Address& hostAddress) {
    if (!sHasInitYojimbo) {
        sHasInitYojimbo = true;
        InitializeYojimbo();
    }

    assert(!sInstance);
    sInstance = new GameClientOLD(connectionType, hostAddress);
    return* sInstance;
}

GameClientOLD& GameClientOLD::getInstance() {
    return *sInstance;
}

void GameClientOLD::destroyInstance() {
    assert(sInstance);
    delete sInstance;
    sInstance = nullptr;
}

GameClientOLD::~GameClientOLD() {
    disconnect();
}

// To enable this we need to use a matcher service on a linux machine
#define USE_SECURE_CONNECT 0 

void GameClientOLD::connect(const uint8_t privateKey[]) {

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

void GameClientOLD::disconnect() {
    mClient->Disconnect();
}

void GameClientOLD::update(double dtSec) {

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

void GameClientOLD::processMessages() {

    for (int i = 0; i < mConnectionConfig.numChannels; i++) {
        yojimbo::Message* message = mClient->ReceiveMessage(i);
        while (message != NULL) {
            processMessage(message);
            mClient->ReleaseMessage(message);
            message = mClient->ReceiveMessage(i);
        }
    }
}

void GameClientOLD::processMessage(yojimbo::Message* message) {

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

void GameClientOLD::sendPingMessage(f64 timestamp) {
    mLastPingTimeS = timestamp;
    PingMessage* pingMessage = (PingMessage*)mClient->CreateMessage(e_cast(MessageTypes::PING));
    pingMessage->mTimeStamp = timestamp;
    mClient->SendMessage(e_cast(MESSAGE_CHANNELS[pingMessage->GetType()]), pingMessage);
}

void GameClientOLD::processPingMessage(PingMessage* message) {
    mCurrentPingMS = (f32)((yojimbo_time() - message->mTimeStamp) * MS_PER_SECOND_D);
}

void GameClientOLD::processClientBeginMessage(ClientBeginMessage* message) {
    assert(mActiveWorld);
    CliFullECS& cliEcs = (CliFullECS&)mActiveWorld->getECS();
    assert(cliEcs.getLocalPlayer() == entt::null);
    entt::entity playerEntity = cliEcs.createEntityFromSrv((entt::entity)message->mSrvEntityID, message->mPosition, CStrToken("player"));
    assert(false);// Need player ID and stuff
    //cliEcs.createLocalPlayer(playerEntity);
    mIsJoined = true;
}

void GameClientOLD::processEntityCreateMessage(EntityCreateMessage* message) {
    assert(mActiveWorld);
    CliFullECS& cliEcs = (CliFullECS&)mActiveWorld->getECS();
    cliEcs.createEntityFromSrv((entt::entity)message->mSrvEntityID, message->mPosition, message->mEntityToken);
}

void GameClientOLD::processEntityTransformMessage(EntityTransformMessage* message) {
    assert(mActiveWorld);
    CliFullECS& cliEcs = (CliFullECS&)mActiveWorld->getECS();
    entt::entity entity = cliEcs.getEntityFromSrvEntity((entt::entity)message->mSrvEntityID);
    if (entity != entt::null) {
        PhysicsComponent& physCmp = cliEcs.mRegistry.get<PhysicsComponent>(entity);
        physCmp.teleportBottomToPoint(message->mPosition);
        // TODO: message->mRotation
    }
}

void GameClientOLD::processCharacterStateMessage(CharacterStateMessage* message) {
    assert(mActiveWorld);
    CliFullECS& cliEcs = (CliFullECS&)mActiveWorld->getECS();
    entt::entity entity = cliEcs.getEntityFromSrvEntity((entt::entity)message->mSrvEntityID);
    if (entity != entt::null) {
        PhysicsComponent& physCmp = cliEcs.mRegistry.get<PhysicsComponent>(entity);
        CharacterControlComponent& controlCmp = cliEcs.mRegistry.get<CharacterControlComponent>(entity);
        physCmp.teleportBottomToPoint(message->mPosition);
        physCmp.setLinearVelocity(message->mVelocity);
        controlCmp.mControllerAngleRad = message->mControlAngle;
        controlCmp.mDesiredLocomotionMode = (CharacterLocomotionMode)message->mDesiredLocomotionMode;
    }
}

void GameClientOLD::replicatePlayerState() {
    assert(mActiveWorld);
    CliFullECS& cliEcs = (CliFullECS&)mActiveWorld->getECS();
    entt::entity entity = cliEcs.getLocalPlayer();
    if (entity != entt::null) {
        PhysicsComponent& physicsCmp = cliEcs.mRegistry.get<PhysicsComponent>(entity);
        CharacterControlComponent& controlCmp = cliEcs.mRegistry.get<CharacterControlComponent>(entity);
        CliMessageOLD::sendPlayerStateMessage(physicsCmp.getBottomPosition(), physicsCmp.getLinearVelocity(), controlCmp.mControllerAngleRad, e_cast(controlCmp.mDesiredLocomotionMode));
    }
}
