#pragma once
// Lets us query what objects are at a tile that we can interact with or inspect.
 // TODO: Will receive notifications if that tile changes so it can refresh

class ItemStockpile;
class Building;
class Structure;
class Chunk;

#include "tile/TileHandle.h"
#include "actor/ActorTypes.h"

struct WorldObjectQueryData {
    ItemStockpile* mStockpileAtTile = nullptr;
    Building* mBuildingAtTile = nullptr;
    std::vector<EntityDistSortKey> mEntitiesAtTile;
    LiteTileHandle mLiteHandle;
    f32v3 mWorldPos;
    TileRef mTileRef;
    StructureArrayPtr mStructures = {};
    Structure* mSelectedStructure = nullptr;
    std::atomic_bool mIsReady = false;
    std::atomic_bool mIsQuerying = false;
};

class WorldObjectQuery {
    friend class TileInteractPanel;
public:
    WorldObjectQuery();
    ~WorldObjectQuery();

    VORB_NON_COPYABLE_BUT_MOVABLE(WorldObjectQuery);

    // Returns false if we are still in the process of a refresh
    bool tryQuery(const f32v3& worldPos);
    bool tryQuery(LiteTileHandle handle);
    void release() { if (mData) { mData->mTileRef.release(); mData.reset(); } }

    bool isReady() const { assert(mData); return mData->mIsReady; }
    bool isValid() const { return mData && mData->mIsReady && mData->mTileRef.isValid(); }

    ItemStockpile* getStockpile() const { assert(mData); return mData->mStockpileAtTile; }
    Building* getBuilding() const { assert(mData); return mData->mBuildingAtTile; }
    const std::vector<EntityDistSortKey>& getEntities() const { assert(mData); return mData->mEntitiesAtTile; }
    TileHandle getTileHandle() const { assert(mData); return TileHandle(mData->mTileRef.container, mData->mTileRef.index); }
    const f32v2& getTilePos() const { assert(mData); return mData->mWorldPos; }

    // TODO: This isn't exactly thread safe, if the structure(s) are deleted while we hold this handle, we have a dangling pointer
    StructureArrayPtr& getStructures() const { assert(mData); return mData->mStructures; }
    Structure* getSelectedStructure() const { assert(mData); return mData->mSelectedStructure; }
    void setSelectedStructure(Structure* s) { mData->mSelectedStructure = s; }

private:
    void query();
    static void queryInternal(WorldObjectQueryData& data);

    // Shared so we can ensure the handle is not destroyed while we are
    // in a task pool
    std::shared_ptr<WorldObjectQueryData> mData;
};