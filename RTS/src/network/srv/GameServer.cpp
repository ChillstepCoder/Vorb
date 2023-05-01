#include "stdafx.h"

#include "GameServer.h"
#include "SrvAdapter.h"

#include "network/NetworkUtil.h"

#include "ecs/srv/SrvEntityComponentSystem.h"

#include "world/srv/SrvWorldInterface.h"
#include "world/IWorld.h"

#include "network/srv/SrvMessage.h"

#include "ecs/component/ReplicationComponent.h"
#include "ecs/component/EntityDetailsComponent.h"


// TODO fix these things, figure out unicode
void logSrv(const wchar_t* str) {
    //OutputDebugString(str);
    wprintf(str);
}

void logSrv(const std::string& str) {
    std::wstring wstr;
    wstr.assign(str.begin(), str.end());
    wprintf(wstr.data());
}

constexpr int MAX_TICKS_IN_FRAME = 2;
constexpr f32 SERVER_BACKLOG_FASTFORWARD_TIME_SEC = 0.6; // Time differential before we just fast forward to make up for it

GameServer* GameServer::sInstance = nullptr;

GameServer::GameServer(IWorld& world, ServerType serverType) :
    mWorld(world),
    mAdapter(std::make_unique<SrvAdapter>(*this)),
    mServer(yojimbo::GetDefaultAllocator(), DEFAULT_PRIVATE_KEY, initServerAddress(serverType), mConnectionConfig, *mAdapter, 0.0),
    mServerType(serverType) {

    // start the server
    mServer.Start(MAX_CLIENTS);
    if (!mServer.IsRunning()) {
        char buffer[256];
        mServerAddress.ToString(buffer, sizeof(buffer));
        throw std::runtime_error("Could not start server " + std::string(buffer));
    }

    // Cache this after starting
    mServerAddress = mServer.GetAddress();

    // print the port we got in case we used port 0
    char buffer[256];
    mServerAddress.ToString(buffer, sizeof(buffer));
    printf("Server initializing with address %s\n", buffer);

    // TODO: PlayerManager?
    mClientPlayerEntities.resize(MAX_CLIENTS);

}

GameServer& GameServer::initInstance(IWorld& world, ServerType serverType) {

    if (!sHasInitYojimbo) {
        sHasInitYojimbo = true;
        InitializeYojimbo();
    }

    assert(!sInstance);
    sInstance = new GameServer(world, serverType);
    return *sInstance;
}

GameServer& GameServer::getInstance() {
    return *sInstance;
}

void GameServer::destroyInstance()
{
    assert(sInstance);
    delete sInstance;
    sInstance = nullptr;
}

GameServer::~GameServer() {
    mServer.Stop();
}

void GameServer::start() {

    // Loop
    mWantsStart = true;
    mRunning = true;

}

int GameServer::tryTick() {

    // We delay start until the first valid tick to avoid loading backlogs
    if (mWantsStart) {
        mWantsStart = false;
        mTimeSec = yojimbo_time();
    }

    constexpr float fixedDtSec = 1.0f / SERVER_TICK_RATE_HZ;
    mTickTimer.startFrame();
    int tickCount = 0;
    while (mTickTimer.tryTick()) {
        update();
        mTimeSec += fixedDtSec;
    }
    double currentTime = yojimbo_time();
    if (currentTime - mTimeSec >= SERVER_BACKLOG_FASTFORWARD_TIME_SEC) {
        std::cout << "Massive server time backlog detected! ";
        std::cout << mTimeSec << " " << currentTime << std::endl;
        mTimeSec = currentTime; // Fast forward
        return 1;
    }
    return 0;
}

void GameServer::stop() {
    mServer.Stop();
}

void GameServer::clientConnected(int clientIndex) {
    char buffer[512];
    sprintf_s(buffer, "Client %d connected", clientIndex);
    logSrv(buffer);
}

void GameServer::clientDisconnected(int clientIndex) {
    char buffer[512];
    sprintf_s(buffer, "Client %d disconnected", clientIndex);
    logSrv(buffer);
}

void GameServer::update() {
    assert(IS_GAME_THREAD());
    // stop if server is not running
    if (!mServer.IsRunning()) {
        mRunning = false;
        return;
    }

    // Update our connected clients list
    updateConnectedClientBits();

    // update server and process messages
    mServer.AdvanceTime(mTimeSec);
    mServer.ReceivePackets();
    processMessages();

    // Replication
    replicateEntities();

    // ... process client inputs ...
    // ... update game ...
    // ... send game state to clients ...

    mServer.SendPackets();
}

void GameServer::updateConnectedClientBits() {
    ui16 connectedClientBits = 0;
    int numClients = mServer.GetNumConnectedClients();
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        // TODO: We can make IsClientConnected more efficient with a custom yojimbo server implementation
        // that avoids redundant checks
        const ui16 bit = (ui16)(1 << i);
        if (mServer.IsClientConnected(i)) {
            connectedClientBits |= bit;
            if ((mConnectedClientBits & bit) == 0) {
                onClientConnected(i);
            }
        }
        else if (mConnectedClientBits & bit) {
            onClientDisconnected(i);
        }
    }
    mConnectedClientBits = connectedClientBits;
}

void GameServer::processMessages() {
    for (int clientIndex = 0; clientIndex < MAX_CLIENTS; ++clientIndex) {
        if (mServer.IsClientConnected(clientIndex)) {
            for (int channelIndex = 0; channelIndex < mConnectionConfig.numChannels; ++channelIndex) {
                yojimbo::Message* message = mServer.ReceiveMessage(clientIndex, channelIndex);
                while (message != nullptr) {
                    processMessage(clientIndex, message);
                    mServer.ReleaseMessage(clientIndex, message);
                    message = mServer.ReceiveMessage(clientIndex, channelIndex);
                }
            }
        }
    }
}

void GameServer::processMessage(int clientIndex, yojimbo::Message* message) {
    switch (message->GetType()) {
        case (int)MessageTypes::PING:
            processPingMessage(clientIndex, (PingMessage*)message);
            break;
        case (int)MessageTypes::CLIENT_READY_JOIN:
            processClientReadyJoinMessage(clientIndex);
            break;
        case (int)MessageTypes::CLIENT_PLAYER_STATE:
            processClientPlayerStateMessage(clientIndex, (ClientPlayerStateMessage*)message);
            break;
        default:
            break;
    }
}

void GameServer::processPingMessage(int clientIndex, PingMessage* message) {
    // Reply with same message so client can compute ping
    // TODO: Server also compute ping?
    PingMessage* pingMessage = (PingMessage*)mServer.CreateMessage(clientIndex, e_cast(MessageTypes::PING));
    pingMessage->mTimeStamp = message->mTimeStamp;
    mServer.SendMessage(clientIndex, e_cast(MESSAGE_CHANNELS[message->GetType()]), pingMessage);
}

void GameServer::processClientReadyJoinMessage(int clientIndex) {
    // If we haven't already processed this message, then begin the clients world and replicate all state
    for (size_t i = 0; i < mConnectedClients.size(); ++i) {
        if (mConnectedClients[i] == clientIndex) {
            // Don't let a client spam us with join requests
            if (!mConnectedClientFlags[i].isBitSet(ClientFlags::JOINED)) {
                mConnectedClientFlags[i].setBit(ClientFlags::JOINED);

                // Replicate start game state to new client
                replicateStartGameStateToClient(clientIndex);

                // Create client entity post state replicate. Ecs will handle the entity replicate and begin message
                // TODO: Save spawn point
                f32v3 playerPos(WorldData::WORLD_CENTER.x, WorldData::WORLD_CENTER.y, 20.0f);
                mClientPlayerEntities[clientIndex] = ((SrvEntityComponentSystem&)mWorld.getECS()).createPlayerEntity(clientIndex, playerPos);
            }
            break;
        }
    }
}

void GameServer::processClientPlayerStateMessage(int clientIndex, ClientPlayerStateMessage* message) {
    SrvEntityComponentSystem& srvEcs = (SrvEntityComponentSystem&)mWorld.getECS();
    entt::entity entity = mClientPlayerEntities[clientIndex];
    if (entity != entt::null) {
        PhysicsComponent& physCmp = srvEcs.mRegistry.get<PhysicsComponent>(entity);
        CharacterControlComponent& controlCmp = srvEcs.mRegistry.get<CharacterControlComponent>(entity);
        physCmp.setTransform(message->mPosition, 0.0f);
        physCmp.setVelocity(message->mVelocity);
        controlCmp.mControllerAngle = message->mControlAngle;
        controlCmp.mDesiredMode = (CharacterLocomotionMode)message->mDesiredLocomotionMode;
    }
}

yojimbo::Address GameServer::initServerAddress(ServerType serverType)
{
    if (serverType == ServerType::DEV) {
        return yojimbo::Address("127.0.0.1", DEFAULT_SERVER_PORT);
    }
    else if (serverType == ServerType::LAN) {
        return yojimbo::Address(NetworkUtil::getLocalIP().c_str(), DEFAULT_SERVER_PORT);
    }
    else {
        return NetworkUtil::getExternalIP(DEFAULT_SERVER_PORT, true /*ipv6*/);
    }
}

void GameServer::onClientConnected(int clientIndex) {
    mConnectedClients.emplace_back(clientIndex);
    mConnectedClientFlags.emplace_back();
}

void GameServer::onClientDisconnected(int clientIndex) {
    for (size_t i = 0; i < mConnectedClients.size(); ++i) {
        if (mConnectedClients[i] == clientIndex) {
            mConnectedClientFlags[i] = mConnectedClientFlags.back();
            mConnectedClients[i] = mConnectedClients.back();
            mConnectedClientFlags.pop_back();
            mConnectedClients.pop_back();
        }
    }
}

void GameServer::replicateStartGameStateToClient(int clientIndex) {
    SrvEntityComponentSystem& ecs = ((SrvEntityComponentSystem&)mWorld.getECS());

    // Replicate all entites
    auto view = ecs.mRegistry.view<PhysicsComponent, ReplicationComponent, EntityDetailsComponent>();
    for (auto entity : view) {
        PhysicsComponent& physicsCmp = view.get<PhysicsComponent>(entity);
        EntityDetailsComponent& detailsCmp = view.get<EntityDetailsComponent>(entity);
        SrvMessage::sendEntityCreateMessage(clientIndex, entity, detailsCmp.mEntityToken, physicsCmp.getPosition(), physicsCmp.getRotation());
    }

    // TODO: Implement start state for other shit
}

void GameServer::replicateEntities() {
    SrvEntityComponentSystem& ecs = ((SrvEntityComponentSystem&)mWorld.getECS());

    // Replicate all characters
    auto view = ecs.mRegistry.view<PhysicsComponent, ReplicationComponent, CharacterControlComponent>();
    for (auto entity : view) {
        ReplicationComponent& repCmp = view.get<ReplicationComponent>(entity);
        PhysicsComponent& physicsCmp = view.get<PhysicsComponent>(entity);
        CharacterControlComponent& controlCmp = view.get<CharacterControlComponent>(entity);

        for (int clientIndex : mConnectedClients) {
            if (repCmp.shouldReplicateTo(clientIndex)) {
                //SrvMessage::sendEntityTransformMessage(clientIndex, entity, physicsCmp.getPosition(), physicsCmp.getRotation());
                SrvMessage::sendCharacterStateMessage(clientIndex, entity, physicsCmp.getPosition(), physicsCmp.getLinearVelocity(), controlCmp.mControllerAngle, e_cast(controlCmp.mDesiredMode));
            }
        }
    }
}
