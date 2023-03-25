#include "stdafx.h"
#include "TerrainMeshManager.h"

#include "world/IWorld.h"

#include "world/HeightmapTerrainQuadtree.h"

#include <boost/container/flat_map.hpp>

TerrainMeshManager::TerrainMeshManager() {
    // Init terrain
    mTerrainTrees.resize(WORLD_SIZE_TERRAIN_QUADTREES);
    for (size_t i = 0; i < mTerrainTrees.size(); ++i) {
        f32v2 pos((i % WORLD_WIDTH_TERRAIN_QUADTREES) * TERRAIN_QUADTREE_WIDTH, (i / WORLD_WIDTH_TERRAIN_QUADTREES) * TERRAIN_QUADTREE_WIDTH);
        mTerrainTrees[i].init(pos);
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

void TerrainMeshManager::tick() {
    assert(IS_GAME_THREAD());
    const f32v2& loadCenter = sWorld->getLoadCenter();
    for (auto&& terrainQuadtree : mTerrainTrees) {
        terrainQuadtree.update(loadCenter);
    }
}

void TerrainMeshManager::dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius) {
    assert(IS_GAME_THREAD());
    // Only update terrain which was impacted by brush
    for (auto&& quadtree : mTerrainTrees) {
        quadtree.onDataChanged(pos, brushRadius);
    }
}

void TerrainMeshManager::dirtyAllTerrain() {
    assert(IS_GAME_THREAD());
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
        const ui32 terrainTreeIndex = rootPosition.y * WORLD_WIDTH_TERRAIN_QUADTREES + rootPosition.x;
        modifiedLeafNodePositions[terrainTreeIndex].emplace_back(leafPosition);
    }

    for (auto&& it : modifiedLeafNodePositions) {
        for (const i32v2& leafPos : it.second) {
            mTerrainTrees[it.first].markLeafDirty(leafPos * LEAF_DIMS);
        }
    }
}
