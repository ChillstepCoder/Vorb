#pragma once
// Lets us query what objects are at a tile that we can interact with or inspect.
 // TODO: Will receive notifications if that tile changes so it can refresh

class World;
class ItemStockpile;
class Building;
class Structure;
class Chunk;

#include "tile/TileHandle.h"
#include "actor/ActorTypes.h"

class WorldObjectQuery {
    friend class UIInteractMenuPopup;
public:
    WorldObjectQuery(World& world, const f32v3& worldPos);

    VORB_NON_COPYABLE_BUT_MOVABLE(WorldObjectQuery);

    void refresh();
    void release() { mTileRef.release(); }

    bool isValid() const { return mTileRef.isValid(); }

    ItemStockpile* getStockpile() const { return mStockpileAtTile; }
    Building* getBuilding() const { return mBuildingAtTile; }
    const std::vector<EntityDistSortKey>& getEntities() const { return mEntitiesAtTile; }
    TileHandle getTileHandle() const { return TileHandle(mTileRef.container, mTileRef.index); }
    World& getWorld() const { return mWorld; }
    const f32v2& getTilePos() const { return mWorldPos; }

private:
    ItemStockpile* mStockpileAtTile = nullptr;
    Building* mBuildingAtTile = nullptr;
    std::vector<EntityDistSortKey> mEntitiesAtTile;
    World& mWorld;
    f32v3 mWorldPos;
    TileRef mTileRef;
    Structure* mSelectedStructure = nullptr;
};