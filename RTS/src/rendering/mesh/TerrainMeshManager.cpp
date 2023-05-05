#include "stdafx.h"
#include "TerrainMeshManager.h"

#include "world/IWorld.h"

#include "world/HeightmapTerrainQuadtree.h"

#include <boost/container/flat_map.hpp>

TerrainMeshManager::TerrainMeshManager(IWorld& world) : mWorld(world) {
    // Init terrain
    mWidthTerrainTrees = mWorld.getWidthChunks() / CHUNKS_PER_TERRAIN_QUADTREE;
    const i32 WORLD_SIZE_TERRAIN_QUADTREES = SQ(mWidthTerrainTrees);

    mTerrainTrees.reserve(WORLD_SIZE_TERRAIN_QUADTREES);
    for (size_t i = 0; i < WORLD_SIZE_TERRAIN_QUADTREES; ++i) {
        const f32v2 pos((i % mWidthTerrainTrees) * TERRAIN_QUADTREE_WIDTH, (i / mWidthTerrainTrees) * TERRAIN_QUADTREE_WIDTH);
        mTerrainTrees.emplace_back(HeightmapTerrainQuadtree(mWorld, pos));
    }

    // Init events
    IHeightmapGrid::registerIHeightmapGridListeners(mHeightmapGridListeners);
    IHeightmapGrid::addEditVertsListener(mHeightmapGridListeners, [this](const HeightmapGridEvent& editEvent) {
        assert(editEvent.mEventType == HeightmapGridEventType::EditVerts);
        assert(editEvent.mModifiedVerts);
        onTerrainModified(*editEvent.mModifiedVerts);
    });
}

TerrainMeshManager::~TerrainMeshManager() {

}

void TerrainMeshManager::tickGameThread(const f32v2& loadCenter) {
    ASSERT_GAME_THREAD();
    for (auto&& terrainQuadtree : mTerrainTrees) {
        terrainQuadtree.update(loadCenter);
    }
}

void TerrainMeshManager::dirtyAllTerrain() {
    ASSERT_GAME_THREAD();
    // Force all terrain to regenerate
    for (size_t i = 0; i < mTerrainTrees.size(); ++i) {
        mTerrainTrees[i].markDirty();
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
            mTerrainTrees[it.first].markLeafDirty(leafPos * LEAF_DIMS);
        }
    }
}
