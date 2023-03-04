#pragma once

#include "tile/HarvestableSubChunkRegistry.h"

class TileContainer;
struct TileContainerEditEvent;

// Unique identifier for a specific registry so we can BFS with a closed list
typedef std::pair<TileContainerID, ui32> HarvestableRegistryKey;

// TODO: Include items on ground?

// Stores the locations and types of resources
constexpr ui32 TILE_CONTAINER_ITEM_REGISTRY_CELL_WIDTH = SUBCHUNK_WIDTH;
static_assert(SUBCHUNK_WIDTH == 16);

class TileContainerHarvestableRegistry {

public:
    TileContainerHarvestableRegistry();
    ~TileContainerHarvestableRegistry();

    void init(const TileContainer& owner);
    void destroy();
    void refreshFromOwner();
    bool tileHasHarvestable(TileIndex tileIndex, TileHarvestable harvestable) const { return mHarvestables[tileIndex] == harvestable; }
    ui32 getRegistryCount() const { return mRegistryCount; }
    const HarvestableSubchunkRegistry* getRegistries() const { return mRegistries.get(); }
    const HarvestableSubchunkRegistry& getRegistry(size_t i) const { return mRegistries[i]; }

    void debugDraw() const;
    VORB_NON_COPYABLE_BUT_MOVABLE(TileContainerHarvestableRegistry);

    void onTileLayerChanged(TileContainerEditEvent& evnt);

private:
    const TileContainer* mOwner = nullptr;
    std::unique_ptr<HarvestableSubchunkRegistry[]> mRegistries;
    std::unique_ptr<TileHarvestable[]> mHarvestables;
    ui32v3 mRegistriesDims;
    ui32 mRegistryCount;
    ui32 mTotalHarvestables[e_cast(TileHarvestable::COUNT)];
};
