#pragma once
// Lets us query what objects are at a tile that we can interact with or inspect.
 // TODO: Will receive notifications if that tile changes so it can refresh

class World;
class ItemStockpile;

#include "tile/TileHandle.h"
#include "actor/ActorTypes.h"

struct WorldObjectQueryData {
    World* mWorld = nullptr;
    ItemStockpile* mStockpileAtTile = nullptr;
    //Building* mBuildingAtTile = nullptr;
    std::vector<EntityDistSortKey> mEntitiesAtTile;
    LiteTileHandle mLiteHandle;
    f32v3 mWorldPos;
    TileRef mTileRef;
    //StructureArrayPtr mStructures = {};
    //Structure* mSelectedStructure = nullptr;
    std::atomic_bool mIsReady = false;
    std::atomic_bool mIsQuerying = false;
};


typedef std::shared_ptr<class WorldObjectQuery> WorldObjectQueryPtr;

class WorldObjectQuery {
    friend class TileInteractPanel;
    friend class WorldObjectQueryFactory;
public:
    WorldObjectQuery(World& world);
    ~WorldObjectQuery();

    VORB_NON_COPYABLE_BUT_MOVABLE(WorldObjectQuery);

    bool isReady() const { return mIsReady; }
    bool isValid() const { return mIsReady && mTileRef.isValid(); }

    ItemStockpile* getStockpile() const { return mStockpileAtTile; }
    const std::vector<EntityDistSortKey>& getEntities() const { return mEntitiesAtTile; }
    TileContainer* getTileContainer() const { return mTileRef.container; }
    TileHandle getTileHandle() const { return TileHandle(mTileRef.container, mTileRef.index); }
    TileIndex getTileIndex() const { return mTileRef.index; }
    const f32v2& getTilePos() const { return mWorldPos; }

private:
    void query(const WorldObjectQueryPtr& ptr);
    void queryInternal();

    World& mWorld;
    ItemStockpile* mStockpileAtTile = nullptr;
    //Building* mBuildingAtTile = nullptr;
    std::vector<EntityDistSortKey> mEntitiesAtTile;
    LiteTileHandle mLiteHandle;
    f32v3 mWorldPos;
    TileRef mTileRef;
    //StructureArrayPtr mStructures = {};
    //Structure* mSelectedStructure = nullptr;
    std::atomic_bool mIsReady = false;
    std::atomic_bool mIsQuerying = false;
};


class WorldObjectQueryFactory {
public:
    static WorldObjectQueryPtr makeQuery(World& world, const f32v3& worldPos);
    static WorldObjectQueryPtr makeQuery(World& world, LiteTileHandle handle);
};