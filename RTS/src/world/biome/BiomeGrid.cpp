#include "stdafx.h"
#include "BiomeGrid.h"

#include "resources/BiomeRepository.h"

#include "rendering/RenderThreadTasks.h"

BiomeGrid::BiomeGrid(ui32 worldWidthTiles) {
    mWidthVerts = worldWidthTiles / BIOME_VERTEX_STRIDE;
    initInternal();
}

BiomeGrid::~BiomeGrid() {
    if (mBiomeTexture) {
        RenderThreadTasks::getInstance().addShutdownTask([biomeTexture = mBiomeTexture]() {
            glDeleteTextures(1, &biomeTexture);
        });
    }
};

const BiomeDef* BiomeGrid::getBiomeDefAtPoint(f32v2 worldPos) const {
    const i32v2 blVertex = i32v2(worldPos) / BIOME_VERTEX_STRIDE;
    if (blVertex.x < 0 || blVertex.y < 0 || blVertex.x >= (i32)mSpatialGrid.getGridWidthCells() || blVertex.y >= (i32)mSpatialGrid.getGridWidthCells()) {
        return nullptr;
    }

    const BiomeUniqueID uniqueId = mGrid[mSpatialGrid.getIDfromGridXY(blVertex)].biomeUniqueId;
    if (uniqueId == BiomeUniqueID::INVALID) return nullptr;
    return &BiomeRepository::get().getBiomeFromUniqueID(uniqueId);
}

void BiomeGrid::initInternal() {
    assert(mWidthVerts);
    mSpatialGrid.init(BIOME_VERTEX_STRIDE, mWidthVerts);
    mGrid.resize(SQ(mWidthVerts));
    LOG_DEBUG("Biome grid allocated {} mb biome",
        (mGrid.size() * sizeof(BiomeVertex)) / 1024.f / 1024.f);
}
