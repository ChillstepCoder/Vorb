#pragma once

#include "tile/TileSpatialGrid.h"
#include <boost/pool/pool_alloc.hpp>
#include <boost/container/small_vector.hpp>

// TODO: Evaluate this pool vs others
template <typename T>
using TileItemPoolAllocator = boost::fast_pool_allocator<T>;
using EntityPoolAllocator = TileItemPoolAllocator<entt::entity>;

// Preallocate 4 items per tile which should usually result in no additional heap allocations
typedef boost::container::small_vector<entt::entity, 4, EntityPoolAllocator> TileItemList;

class TileItemContainer {
public:
    void init(const TileSpatialGrid* tileSpatialGrid) {
        mTileSpatialGrid = tileSpatialGrid;
        assert(mTileSpatialGrid);
        assert(mTileSpatialGrid->getNumTiles());
        mTileItems.resize(mTileSpatialGrid->getNumTiles());
    }
    // TODO: Serialize
    void destroy() {
        std::vector<TileItemList>().swap(mTileItems);
        mTileSpatialGrid = nullptr;
    }
protected:
    const TileSpatialGrid* mTileSpatialGrid = nullptr;
    std::vector<TileItemList> mTileItems;
};