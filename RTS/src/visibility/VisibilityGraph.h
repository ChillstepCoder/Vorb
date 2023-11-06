#pragma once

#include <boost/pool/pool_alloc.hpp>
#include <boost/container/small_vector.hpp>

#include "tile/TileSpatialGrid.h"
#include "item/ItemStack.h"
#include "util/BitArray.h"

typedef ui16 VisibilityNodeIndex;

// TODO: Evaluate this pool vs others
template <typename T>
using TileItemPoolAllocator = boost::fast_pool_allocator<T>;
using EntityPoolAllocator = TileItemPoolAllocator<entt::entity>;

// 2x2 tiles
struct VisiblityGraphNodeEntities {
    boost::container::small_vector<entt::entity, 4, EntityPoolAllocator> items;
    boost::container::small_vector<entt::entity, 4, EntityPoolAllocator> entities;
};

// Allows querying visibility of entities and items
// via BFS
class TileContainerVisibilityGraph {
public:
    TileContainerVisibilityGraph();

protected:
    TileContainerID mContainerId;
    // 0 bit means empty node, 1 means non-empty node
    TileSpatialGrid mSpatialGrid;
    // Fast empty test, as most nodes are empty. Prevents accessing mNodes
    BitArray mNonEmptyNodesLookup;
    // Entities contained in each node
    std::vector<VisiblityGraphNodeEntities> mNodeEntities;
    // 1 Bit indicates visible neighbor node
    //  67
    // 4xx5
    // 2xx3
    //  01
    std::vector<ui8> mNodeEdges;
    // Map tile to visibility node
    std::vector<VisibilityNodeIndex> mTileNodeIndices;
};
