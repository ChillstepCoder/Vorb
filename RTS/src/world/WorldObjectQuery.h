#pragma once
// Lets us query what objects are at a tile that we can interact with or inspect.
 // TODO: Will receive notifications if that tile changes so it can refresh

class World;
class ItemStockpile;
class Building;
class Chunk;

#include "world/TileHandle.h"
#include "actor/ActorTypes.h"

class WorldObjectQuery {
    friend class UIInteractMenuPopup;
public:
    WorldObjectQuery(World& world, const f32v2& tilePos);

    VORB_NON_COPYABLE_BUT_MOVABLE(WorldObjectQuery);

    void refresh();
    void release() { mTileRef.release(); }

    bool isValid() const { return mTileRef.isValid(); }

    ItemStockpile* getStockpile() const { return mStockpileAtTile; }
    Building* getBuilding() const { return mBuildingAtTile; }
    const std::vector<EntityDistSortKey>& getEntities() const { return mEntitiesAtTile; }
    TileHandle getTileHandle() const { return TileHandle(mTileRef.chunk, mTileRef.index); }
    World& getWorld() const { return mWorld; }
    const f32v2& getTilePos() const { return mTilePos; }

private:
    ItemStockpile* mStockpileAtTile = nullptr;
    Building* mBuildingAtTile = nullptr;
    std::vector<EntityDistSortKey> mEntitiesAtTile;
    World& mWorld;
    f32v2 mTilePos;
    TileRef mTileRef;
};