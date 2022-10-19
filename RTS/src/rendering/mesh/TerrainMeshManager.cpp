#include "stdafx.h"
#include "TerrainMeshManager.h"

#include "world/IWorld.h"

#include "world/HeightmapTerrainQuadtree.h"

TerrainMeshManager::TerrainMeshManager() {
    // Init terrain
    mTerrainTrees.resize(WORLD_SIZE_TERRAIN_QUADTREES);
    for (size_t i = 0; i < mTerrainTrees.size(); ++i) {
        f32v2 pos((i % WORLD_WIDTH_TERRAIN_QUADTREES) * TERRAIN_QUADTREE_WIDTH, (i / WORLD_WIDTH_TERRAIN_QUADTREES) * TERRAIN_QUADTREE_WIDTH);
        mTerrainTrees[i].init(pos);
    }
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
    // Only update terrain which was impacted by brush
    for (auto&& quadtree : mTerrainTrees) {
        quadtree.onDataChanged(pos, brushRadius);
    }
}

void TerrainMeshManager::dirtyAllTerrain() {
    // Force all terrain to regenerate
    for (size_t i = 0; i < mTerrainTrees.size(); ++i) {
        mTerrainTrees[i].markDirty();
    }
}
