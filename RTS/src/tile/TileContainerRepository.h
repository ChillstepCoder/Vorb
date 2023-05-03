#pragma once

#include "tile/TileContainerConst.h"
#include "tile/TileContainerEvents.h"

class IWorld;
class TileContainer;

typedef std::unordered_map<TileContainerID, std::unique_ptr<TileContainer>> TileContainerMap;

class TileContainerRepository {
public:
    TileContainerRepository(IWorld& world);
    ~TileContainerRepository();

    TileContainer* getNewTileContainer(const ui32v3& rootPos, const ui32v3& dims, ui32 floorHeight, VarTileContainerOwner owner);
    void destroyTileContainer(TileContainer* container);

    TileContainer* getTileContainer(TileContainerID id);
    TileContainer* tryGetTileContainer(TileContainerID id);

    const TileContainerMap& getTileContainers() const { /*ASSERT_GAME_THREAD();*/return mTileContainers; }

    // TODO: NON STATIC
    STATIC_EVENT_LISTENER_FUNCS(TileContainer, LoadFinished, TileContainerEventType::LoadFinished, const TileContainerEvent&);
    STATIC_EVENT_LISTENER_FUNCS(TileContainer, Ready, TileContainerEventType::Ready, const TileContainerEvent&);
    STATIC_EVENT_LISTENER_FUNCS(TileContainer, EditTiles, TileContainerEventType::EditTiles, const TileContainerEvent&);
    STATIC_EVENT_LISTENER_FUNCS(TileContainer, Destroy, TileContainerEventType::Destroy, const TileContainerEvent&);
    STATIC_EVENT_DISPATCHER_DEF(TileContainer);
private:

    std::mutex mMutex;
    TileContainerMap mTileContainers;
    TileContainerID mTileContainerIdGen = 0;
    IWorld& mWorld;
};