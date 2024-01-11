#include "stdafx.h"
#include "TerrainMeshManager.h"

#include "world/World.h"

#include "world/HeightmapTerrainQuadtree.h"

#include <boost/container/flat_map.hpp>

TerrainMeshManager::TerrainMeshManager(World& world) : mWorld(world) {
    // Init terrain
    mWidthTerrainTrees = mWorld.getWidthChunks() / CHUNKS_PER_TERRAIN_QUADTREE;
    IHeightmapGrid& heightmapGrid = mWorld.getHeightmapGrid();
    const i32 WORLD_SIZE_TERRAIN_QUADTREES = SQ(mWidthTerrainTrees);

    mTerrainTrees.reserve(WORLD_SIZE_TERRAIN_QUADTREES);
    for (size_t i = 0; i < WORLD_SIZE_TERRAIN_QUADTREES; ++i) {
        const f32v2 pos((i % mWidthTerrainTrees) * TERRAIN_QUADTREE_WIDTH, (i / mWidthTerrainTrees) * TERRAIN_QUADTREE_WIDTH);
        mTerrainTrees.emplace_back(HeightmapTerrainQuadtree(mWorld, pos));
    }

    // Init events
    heightmapGrid.registerIHeightmapGridListeners(mHeightmapGridListeners);
    heightmapGrid.addEditVertsListener(mHeightmapGridListeners, [this](const HeightmapGridEvent& editEvent) {
        assert(editEvent.mEventType == HeightmapGridEventType::EditVerts);
        assert(editEvent.mModifiedVerts);
        onTerrainModified(*editEvent.mModifiedVerts);
    });
}

TerrainMeshManager::~TerrainMeshManager() {
    // TODO: Crash here due to mHeightmapGridListeners during world destroy
}

void TerrainMeshManager::shutdown() {
    mHeightmapGridListeners.reset();
    mDidShutdown = true;
}

void TerrainMeshManager::frameUpdate(const f32v2& loadCenter, f32 elapsedSec) {
    ASSERT_RENDER_THREAD();

    assert(!mDidShutdown);
    // Dirty nodes
    constexpr size_t BULK_SIZE = 128;
    DirtyTreeNode dirtyNodes[BULK_SIZE];
    if (size_t count = mDirtyNodesQueue.try_dequeue_bulk(dirtyNodes, BULK_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            auto&& data = dirtyNodes[i];
            mTerrainTrees[data.terrainTreeIndex].markLeafDirty(data.leafPos);
        }
    }

    for (auto&& terrainQuadtree : mTerrainTrees) {
        terrainQuadtree.update(loadCenter, elapsedSec);
    }
}

void TerrainMeshManager::onTerrainModified(const boost::container::flat_set<i32v2>& modifiedPositions) {
    PROFILE_FUNCTION();
    const i32v2 ROOT_DIMS = HeightmapTerrainQuadtree::LOD_DIMS[0].xy;
    const f32v2 ROOT_HALF_DIMSF = HeightmapTerrainQuadtree::LOD_DIMS[0].xy / 2u;
    const i32v2 LEAF_DIMS = HeightmapTerrainQuadtree::LOD_DIMS[HeightmapTerrainQuadtree::HIGHEST_LOD].xy;
    // Condense all updates to just the leaf positions
    boost::container::flat_map<ui32 /*terrainTreeIndex*/, std::vector<f32v2>> modifiedLeafNodePositions;
    for (const i32v2& modifiedPos : modifiedPositions) {
        const i32v2 rootPosition = modifiedPos / ROOT_DIMS;
        const i32v2 leafPosition = modifiedPos / LEAF_DIMS;
        const ui32 terrainTreeIndex = rootPosition.y * mWidthTerrainTrees + rootPosition.x;
        modifiedLeafNodePositions[terrainTreeIndex].emplace_back(leafPosition);
    }

    for (auto&& it : modifiedLeafNodePositions) {
        for (const i32v2& leafPos : it.second) {
            mDirtyNodesQueue.enqueue(DirtyTreeNode{ it.first, leafPos * LEAF_DIMS });
        }
    }
}
