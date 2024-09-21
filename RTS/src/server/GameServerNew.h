#pragma once

#include "world/srv/ServerChunkStateEvents.h"

class ServerReportStateManager;
class ServerChunkAuthorityManager;
class World;
class ServerThread;

class GameServerNew {
public:
    // TODO: SrvWorld?
    GameServerNew(World& world);
    ~GameServerNew();

    void start();
    void stop();

    VORB_NON_COPYABLE(GameServerNew);

    ServerPlayerID initLocalPlayer(f32v3 position);
    void setLocalPlayerPosition(f32v3 position);

    ServerPlayerID getLocalPlayerId() const { return mLocalPlayerId; }

private:
    friend class ServerThread;

    World& mWorld;
    std::unique_ptr<ServerThread> mServerThread;
    std::unique_ptr<ServerReportStateManager> mPlayerManager;
    std::unique_ptr<ServerChunkAuthorityManager> mAuthorityManager;
    ServerPlayerID mLocalPlayerId;

    ServerChunkAuthorityManagerListeners mAuthorityManagerListeners;
    std::unique_ptr<moodycamel::ProducerToken> mServerThreadProducerToken;
};

