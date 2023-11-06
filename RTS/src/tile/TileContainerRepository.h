#pragma once

#include "tile/TileContainerConst.h"
#include "tile/TileContainerEvents.h"

class World;
class TileContainer;
class TileContainerLoader;

typedef std::unordered_map<TileContainerID, std::unique_ptr<TileContainer>> TileContainerMap;

enum class LoadBehavior {
    LOAD_OR_GENERATE,
    CREATE_EMPTY
};

class TileContainerRepository {
public:
    TileContainerRepository(World& world);
    ~TileContainerRepository();

    TileContainer* loadTerrainTileContainer(ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, Chunk* owner);
    // Instantly initialized and valid
    TileContainer* createNewEmptyBuildingContainer(ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, Building* owner);

    void destroyTileContainer(TileContainer* container);

    TileContainer* getTileContainer(TileContainerID id);
    TileContainer* tryGetTileContainer(TileContainerID id);

    const TileContainerMap& getTileContainers() const { /*ASSERT_GAME_THREAD();*/return mTileContainers; }

    // TODO: NON STATIC
    EVENT_LISTENER_FUNCS(TileContainer, LoadFinished, TileContainerEventType::LoadFinished, const TileContainerEvent&);
    EVENT_LISTENER_FUNCS(TileContainer, Ready, TileContainerEventType::Ready, const TileContainerEvent&);
    EVENT_LISTENER_FUNCS(TileContainer, EditTiles, TileContainerEventType::EditTiles, const TileContainerEvent&);
    EVENT_LISTENER_FUNCS(TileContainer, TileDamaged, TileContainerEventType::TileDamaged, const TileContainerEvent&);
    EVENT_LISTENER_FUNCS(TileContainer, TileDestroyed, TileContainerEventType::TileDestroyed, const TileContainerEvent&);
    EVENT_LISTENER_FUNCS(TileContainer, Destroy, TileContainerEventType::Destroy, const TileContainerEvent&);
    EVENT_DISPATCHER_DEF(TileContainer);
private:
    TileContainer* allocateNewTileContainer(ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, VarTileContainerOwner owner);

    std::mutex mMutex;
    TileContainerMap mTileContainers;
    TileContainerID mTileContainerIdGen = 0;
    World& mWorld;
    std::unique_ptr<TileContainerLoader> mLoader;
};