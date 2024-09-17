#include "stdafx.h"
#include "BiomeGrid.h"

#include "resources/BiomeRepository.h"

#include "rendering/RenderThreadTasks.h"

LivingBiomeInstance::LivingBiomeInstance(LivingBiomeID id, LivingBiomeType type, ui8 powerLevel, BlockCoord rootPos) :
    mId(id),
    mType(type),
    mPowerLevel(powerLevel),
    mSizeBlocks(1),
    mRootBlockPos(rootPos)
{

}


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

const BiomeDef* BiomeGrid::getBiomeDefAtPoint(TileCoord worldPos) const {
    const BlockCoord blockPos(worldPos);
    if (blockPos.x < 0 || blockPos.y < 0 || blockPos.x >= mWidthVerts || blockPos.y >= mWidthVerts) [[unlikely]] {
        return nullptr;
    }

    i32v2 cellOffset;
    ui32 id = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v, cellOffset);

    const BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
    BiomeUniqueID uniqueId;
    {
        std::lock_guard lock(mMutex);
        uniqueId = vertex.biomeUniqueId;
    }

    if (uniqueId == BiomeUniqueID::INVALID) [[unlikely]] {
        return nullptr;
    }
    return &BiomeRepository::get().getBiomeFromUniqueID(uniqueId);
}

BiomeUniqueID BiomeGrid::tryMutateBiomeAtTile(TileCoord worldPos, LivingBiomeType type) {
    return tryMutateBiomeAtBlockPos(BlockCoord(worldPos), type);
}

BiomeUniqueID BiomeGrid::tryMutateBiomeAtBlockPos(BlockCoord blockPos, LivingBiomeType type) {
    if (blockPos.x < 0 || blockPos.y < 0 || blockPos.x >= mWidthVerts || blockPos.y >= mWidthVerts) [[unlikely]] {
        return BiomeUniqueID::INVALID;
    }

    i32v2 cellOffset;
    const ui32 id = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v, cellOffset);

    BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
    BiomeUniqueID uniqueId;
    {
        std::lock_guard lock(mMutex);
        uniqueId = vertex.biomeUniqueId;
        if (uniqueId == BiomeUniqueID::INVALID) [[unlikely]] {
            return BiomeUniqueID::INVALID;
        }
        const BiomeDef& def = BiomeRepository::get().getBiomeFromUniqueID(uniqueId);
        if (def.isCorruptable) {
            vertex.biomeUniqueId = def.mutatedVersions[e_cast(type)]->uniqueId;
            return vertex.biomeUniqueId;
        }
    }
    return BiomeUniqueID::INVALID;
}

bool BiomeGrid::trySpawnLivingBiomeAtBlockPos(BlockCoord blockPos, LivingBiomeType type) {
    if (blockPos.x < 0 || blockPos.y < 0 || blockPos.x >= mWidthVerts || blockPos.y >= mWidthVerts) [[unlikely]] {
        return false;
    }

    i32v2 cellOffset;
    ui32 id = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v, cellOffset);

    BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
    BiomeUniqueID uniqueId;
    {
        std::lock_guard lock(mMutex);
        if (vertex.livingBiomeId != INVALID_LIVING_BIOME_ID) {
            return false;
        }
        uniqueId = vertex.biomeUniqueId;
        const BiomeDef& def = BiomeRepository::get().getBiomeFromUniqueID(uniqueId);
        if (def.isCorruptable) {
            vertex.biomeUniqueId = def.mutatedVersions[e_cast(type)]->uniqueId;
            LivingBiomeID newId = mLivingBiomes.size();
            mLivingBiomes.emplace_back(newId, type, 0, blockPos);
            return true;
        }
    }
    return false;
}

BiomeVertex& BiomeGrid::getVertexForGenerationFromBlockPos(BlockCoord blockPos) {
    i32v2 cellOffset;
    const ui32 id = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v, cellOffset);
    return mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
}

void BiomeGrid::initInternal() {
    assert(mWidthVerts);
    const ui32 widthCells = mWidthVerts / BIOME_PATCH_WIDTH_VERTS;
    mSpatialGrid.init(BIOME_PATCH_WIDTH_VERTS, widthCells);
    mGrid.resize(mSpatialGrid.getGridSizeCells());
    mPatchSavesUpToDate = std::make_unique<std::atomic_flag[]>(mSpatialGrid.getGridSizeCells());
    LOG_DEBUG("Biome grid allocated {} mb biome", (mGrid.size() * sizeof(BiomePatch)) / 1024.f / 1024.f);
}
