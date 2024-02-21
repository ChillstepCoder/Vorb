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

const BiomeDef* BiomeGrid::getBiomeDefAtPoint(i32v2 worldPos) const {

    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWidthVerts * BIOME_VERTEX_STRIDE || worldPos.y >= mWidthVerts * BIOME_VERTEX_STRIDE) [[unlikely]] {
        return nullptr;
    }

    i32v2 cellOffset;
    ui32 id = mSpatialGrid.getIDAndCellOffsetAtWorldPos(worldPos, cellOffset);
    cellOffset /= BIOME_VERTEX_STRIDE;

    const BiomeUniqueID uniqueId = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x].biomeUniqueId;
    if (uniqueId == BiomeUniqueID::INVALID) [[unlikely]] {
        return nullptr;
    }
    return &BiomeRepository::get().getBiomeFromUniqueID(uniqueId);
}

BiomeVertex& BiomeGrid::getVertexForGenerationFromBlockPos(i32v2 blockPos) {
    i32v2 cellOffset;
    ui32 id = mSpatialGrid.getIDAndCellOffsetAtWorldPos(blockPos * BLOCK_WIDTH, cellOffset);
    cellOffset /= BIOME_VERTEX_STRIDE;

    return mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
}

void BiomeGrid::initInternal() {
    assert(mWidthVerts);
    mSpatialGrid.init(BIOME_PATCH_WIDTH_VERTS * BLOCK_WIDTH, mWidthVerts / BIOME_PATCH_WIDTH_VERTS);
    mGrid.resize(mSpatialGrid.getGridSizeCells());
    mPatchSavesUpToDate = std::make_unique<std::atomic_flag[]>(mSpatialGrid.getGridSizeCells());
    LOG_DEBUG("Biome grid allocated {} mb biome",
        (mGrid.size() * sizeof(BiomePatch)) / 1024.f / 1024.f);
}
