#include "stdafx.h"
#include "BiomeGrid.h"

#include "resources/BiomeRepository.h"

#include "rendering/RenderThreadTasks.h"

constexpr TimestampMs GROWTH_INTERVAL_MS = 2000;

LivingBiomeInstance::LivingBiomeInstance(LivingBiomeID id, LivingBiomeType type, ui8 powerLevel, BlockCoord rootPos) :
    mId(id),
    mType(type),
    mPowerLevel(powerLevel),
    mSizeBlocks(1),
    mRootBlockPos(rootPos) {
    mEdgeBlockPositions.reserve(64);
    mEdgeBlockPositions.push_back(rootPos);
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

void BiomeGrid::updateGrowth(TimestampMs simTime) {

    // Helper
    auto grow = [this](BlockCoord pos, const BiomeVertex& sourceVertex, LivingBiomeInstance& livingBiome) {
        const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(pos.v);
        BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
        const BiomeDef& def = BiomeRepository::get().getBiomeFromUniqueID(vertex.biomeUniqueId);
        BiomeUniqueID newUniqueId = def.mutatedVersions[e_cast(livingBiome.mType)]->uniqueId;
        { // Critical section
            std::lock_guard lock(mPatchMutexes[id]);
            vertex.biomeFlags.clearBit(BiomeFlags::Corruptable);
            vertex.biomeUniqueId = newUniqueId;
            vertex.distanceFromLivingRoot = sourceVertex.distanceFromLivingRoot + 1;
            vertex.livingBiomeId = sourceVertex.livingBiomeId;
        }
    };

    // Step until we are caught up
    while (simTime >= mNextGrowthTime) {
        PROFILE_FUNCTION();
        assert(IS_GENERATION_THREAD() || IS_SIM_THREAD());

        mNextGrowthTime += GROWTH_INTERVAL_MS;
        // Determine growth outside critical section
        for (LivingBiomeInstance& biome : mLivingBiomes) {
            // Asymptotic probability, bigger we are, less chance of growth
            // https://www.desmos.com/calculator/oehxxadtfu
            constexpr f32 probabilityFalloff = 0.1f; // Larger this is, faster probability decays with size
            const f32 sizep1 = biome.mSizeBlocks * probabilityFalloff + 1.f;
            const f32 growthChance = 2.f * sizep1 / (SQ(sizep1) + 1.f);
            if (mGrowthGen.getRandomFloatSigned() > growthChance) {
                continue;
            }
            for (int i = (int)biome.mEdgeBlockPositions.size(); i >= 0; --i) {
                const BlockCoord edgePos = biome.mEdgeBlockPositions[i];
                const auto [sourceId, sourceCellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(edgePos.v);
                const BiomeVertex& sourceVertex = mGrid[sourceId][sourceCellOffset.y * BIOME_PATCH_WIDTH_VERTS + sourceCellOffset.x];
                // Check a random direction
                const ui32 dir = mGrowthGen.getRandomUIntInRange(0u, 4u);
                switch (dir) {
                    case 0:
                    {
                        BlockCoord south(edgePos.x, edgePos.y - 1);
                        if (south.y >= 0) {
                            grow(south, sourceVertex, biome);
                        }
                    }
                    case 1:
                    {
                        BlockCoord west(edgePos.x - 1, edgePos.y);
                        if (west.x >= 0) {
                            grow(west, sourceVertex, biome);
                        }
                    }
                    case 2:
                    {
                        BlockCoord east(edgePos.x + 1, edgePos.y);
                        if (east.x < mWidthVerts) {
                            grow(east, sourceVertex, biome);
                        }
                    }
                    case 3:
                    {
                        BlockCoord north(edgePos.x, edgePos.y + 1);
                        if (north.y < mWidthVerts) {
                            grow(north, sourceVertex, biome);
                        }
                    }
                }
            }
        }
    }
}

const BiomeDef* BiomeGrid::getBiomeDefAtPoint(TileCoord worldPos) const {
    const BlockCoord blockPos(worldPos);
    if (blockPos.x < 0 || blockPos.y < 0 || blockPos.x >= mWidthVerts || blockPos.y >= mWidthVerts) [[unlikely]] {
        return nullptr;
    }

    const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v);

    const BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
    BiomeUniqueID uniqueId;
    {
        std::lock_guard lock(mPatchMutexes[id]);
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
    assert(IS_GENERATION_THREAD() || IS_SIM_THREAD());

    if (blockPos.x < 0 || blockPos.y < 0 || blockPos.x >= mWidthVerts || blockPos.y >= mWidthVerts) [[unlikely]] {
        return BiomeUniqueID::INVALID;
    }

    const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v);

    BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
    BiomeUniqueID uniqueId = vertex.biomeUniqueId;
    if (vertex.biomeFlags.isBitSet(BiomeFlags::Corruptable)) {
        const BiomeDef& def = BiomeRepository::get().getBiomeFromUniqueID(uniqueId);
        uniqueId = def.mutatedVersions[e_cast(type)]->uniqueId;
        {
            std::lock_guard lock(mPatchMutexes[id]);
            vertex.biomeUniqueId = uniqueId;
        }
        return vertex.biomeUniqueId;
    }
    return BiomeUniqueID::INVALID;
}

bool BiomeGrid::trySpawnLivingBiomeAtBlockPos(BlockCoord blockPos, LivingBiomeType type) {
    assert(IS_GENERATION_THREAD() || IS_SIM_THREAD());
    if (blockPos.x < 0 || blockPos.y < 0 || blockPos.x >= mWidthVerts || blockPos.y >= mWidthVerts) [[unlikely]] {
        return false;
    }

    const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v);
    BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];

    if (vertex.biomeFlags.isBitUnset(BiomeFlags::Corruptable)) {
        return false;
    }

    const BiomeDef& def = BiomeRepository::get().getBiomeFromUniqueID(vertex.biomeUniqueId);
    assert(def.isCorruptable);
    LivingBiomeID newId = mLivingBiomes.size();
    BiomeUniqueID newBiome = def.mutatedVersions[e_cast(type)]->uniqueId;
    { // Critical section
        std::lock_guard lock(mPatchMutexes[id]);
        vertex.biomeUniqueId = newBiome;
        vertex.distanceFromLivingRoot = 0;
        vertex.livingBiomeId = newId;
        mLivingBiomes.emplace_back(newId, type, 0, blockPos);
    }
    return true;
}

BiomeVertex& BiomeGrid::getVertexForGenerationFromBlockPos(BlockCoord blockPos) {
    const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v);
    return mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
}

void BiomeGrid::initInternal() {
    assert(mWidthVerts);
    const ui32 widthCells = mWidthVerts / BIOME_PATCH_WIDTH_VERTS;
    mSpatialGrid.init(BIOME_PATCH_WIDTH_VERTS, widthCells);
    mGrid.resize(mSpatialGrid.getGridSizeCells());
    mPatchMutexes = std::make_unique<std::mutex[]>(mSpatialGrid.getGridSizeCells());
    mPatchSavesUpToDate = std::make_unique<std::atomic_flag[]>(mSpatialGrid.getGridSizeCells());
    mBiomeGrowthClosedList.resizeAndZero(SQ(mWidthVerts));
    LOG_DEBUG("Biome grid allocated {} mb biome", (mSpatialGrid.getGridSizeCells() * sizeof(BiomePatch)) / 1024.f / 1024.f);
}

bool BiomeGrid::canBlockBeCorrupted(BlockCoord blockPos, LivingBiomeType type) {
    const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v);
    const BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
    return vertex.biomeFlags.isBitSet(BiomeFlags::Corruptable);
}
