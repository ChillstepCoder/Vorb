#pragma once

#include "world/srv/ServerChunkStateEvents.h"

class ServerReportStateManager;
class ServerChunkAuthorityManager;
class World;

class GameServerNew {
public:
    // TODO: SrvWorld?
    GameServerNew(World& world);
    ~GameServerNew();

    VORB_NON_COPYABLE(GameServerNew);

    void tick(f64 deltaTime);

    ServerPlayerID initLocalPlayer(f32v3 position);
    void setLocalPlayerPosition(f32v3 position);

    ServerPlayerID getLocalPlayerId() const { return mLocalPlayerId; }

private:
    World& mWorld;

    std::unique_ptr<ServerReportStateManager> mPlayerManager;
    std::unique_ptr<ServerChunkAuthorityManager> mAuthorityManager;
    ServerPlayerID mLocalPlayerId;

    ServerChunkAuthorityManagerListeners mAuthorityManagerListeners;
};

