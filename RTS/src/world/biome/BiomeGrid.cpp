#include "stdafx.h"
#include "BiomeGrid.h"

#include "resources/BiomeRepository.h"

#include "rendering/RenderThreadTasks.h"

constexpr TimestampMs GROWTH_INTERVAL_MS = 20000;

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

void BiomeGrid::updateSimThread(TimestampMs simTime) {

    struct BiomeAtCoord {
        BlockCoord coord;
        BiomeUniqueID id;
    };

    std::vector<BiomeAtCoord> changedThisFrame;

    // Helper
    auto tryGrow = [this, &changedThisFrame](BlockCoord pos, const BiomeVertex& sourceVertex, LivingBiomeInstance& livingBiome) {
        const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(pos.v);
        BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
        if (vertex.biomeFlags.isBitSet(BiomeFlags::Corruptable)) {
            const BiomeDef& def = BiomeRepository::get().getBiomeFromUniqueID(vertex.biomeUniqueId);
            BiomeUniqueID newUniqueId = def.mutatedVersions[e_cast(livingBiome.mType)]->uniqueId;
            { // Critical section
                std::lock_guard lock(mPatchMutexes[id]);
                vertex.biomeFlags.clearBit(BiomeFlags::Corruptable);
                vertex.biomeUniqueId = newUniqueId;
                vertex.distanceFromLivingRoot = sourceVertex.distanceFromLivingRoot + 1;
                vertex.livingBiomeId = sourceVertex.livingBiomeId;
            }
            livingBiome.mEdgeBlockPositions.emplace_back(pos);
            // We had an invalid one
            assert(pos.x >= 0 && pos.y >= 0 && pos.x < mWidthVerts && pos.y < mWidthVerts);
            ++livingBiome.mSizeBlocks;
            BiomeGridEvent event;
            event.blockPos = pos;
            event.livingBiomeType = livingBiome.mType;
            event.biomeUniqueId = newUniqueId;
            dispatchOnCorruption(event);

            changedThisFrame.push_back({ pos, newUniqueId });
        }
    };

    // Step until we are caught up
    while (simTime >= mNextGrowthTime) {
        changedThisFrame.reserve(changedThisFrame.size() + 64);
        LOG_DEBUG(" SIM STEP {}  {}", mNextGrowthTime, simTime);
        PROFILE_FUNCTION();
        assert(IS_SIM_THREAD());

        mNextGrowthTime += GROWTH_INTERVAL_MS;
        // Determine growth outside critical section
        for (LivingBiomeInstance& biome : mLivingBiomes) {
            // Asymptotic probability, bigger we are, less chance of growth
            // https://www.desmos.com/calculator/oehxxadtfu
            constexpr f32 probabilityFalloff = 0.02f; // Larger this is, faster probability decays with size
            const f32 sizep1 = biome.mSizeBlocks * probabilityFalloff + 1.f;
            const f32 growthChance = 2.f * sizep1 / (SQ(sizep1) + 1.f);
            if (mGrowthGen.getRandomFloatUnsigned() > growthChance) {
                continue;
            }
            for (int i = (int)biome.mEdgeBlockPositions.size() - 1; i >= 0; --i) {
                const BlockCoord edgePos = biome.mEdgeBlockPositions[i];
                const auto [sourceId, sourceCellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(edgePos.v);
                const BiomeVertex& sourceVertex = mGrid[sourceId][sourceCellOffset.y * BIOME_PATCH_WIDTH_VERTS + sourceCellOffset.x];
                // Check a random direction
                const ui32 dir = mGrowthGen.getRandomUIntInRange(0u, 4u);
                switch (dir) {
                    case 0: {
                        BlockCoord south(edgePos.x, edgePos.y - 1);
                        if (south.y >= 0) {
                            tryGrow(south, sourceVertex, biome);
                        }
                        break;
                    }
                    case 1: {
                        BlockCoord west(edgePos.x - 1, edgePos.y);
                        if (west.x >= 0) {
                            tryGrow(west, sourceVertex, biome);
                        }
                        break;
                    }
                    case 2: {
                        BlockCoord east(edgePos.x + 1, edgePos.y);
                        if (east.x < mWidthVerts) {
                            tryGrow(east, sourceVertex, biome);
                        }
                        break;
                    }
                    case 3: {
                        BlockCoord north(edgePos.x, edgePos.y + 1);
                        if (north.y < mWidthVerts) {
                            tryGrow(north, sourceVertex, biome);
                        }
                        break;
                    }
                }

                // Check if we are no longer an edge
                BlockCoord south(edgePos.x, edgePos.y - 1);
                if (south.y >= 0) {
                    if (canBlockBeCorruptedInternal(south, biome.mType)) {
                        continue;
                    }
                }
                BlockCoord west(edgePos.x - 1, edgePos.y);
                if (west.x >= 0) {
                    if (canBlockBeCorruptedInternal(west, biome.mType)) {
                        continue;
                    }
                }
                BlockCoord east(edgePos.x + 1, edgePos.y);
                if (east.x < mWidthVerts) {
                    if (canBlockBeCorruptedInternal(east, biome.mType)) {
                        continue;
                    }
                }
                BlockCoord north(edgePos.x, edgePos.y + 1);
                if (north.y < mWidthVerts) {
                    if (canBlockBeCorruptedInternal(north, biome.mType)) {
                        continue;
                    }
                }

                // We are no longer an edge
                biome.mEdgeBlockPositions[i] = biome.mEdgeBlockPositions.back();
                biome.mEdgeBlockPositions.pop_back();
            }
        }
    }

    if (changedThisFrame.size()) {
        assert(mBiomeTexture);
        RenderThreadTasks::getInstance().addGenericTask([this, changed = std::move(changedThisFrame)]() {
            for (const BiomeAtCoord& change : changed) {
                glTextureSubImage2D(mBiomeTexture, 0, change.coord.x, change.coord.y, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &change.id);
            }
        });
    }
}

bool BiomeGrid::canBlockBeCorrupted(BlockCoord blockPos, LivingBiomeType type) {
    const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v);
    const BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
    {
        std::shared_lock lock(mPatchMutexes[id]);
        return vertex.biomeFlags.isBitSet(BiomeFlags::Corruptable);
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
        std::shared_lock lock(mPatchMutexes[id]);
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
    assert(IS_SIM_THREAD());

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
        assert(mBiomeTexture);
        RenderThreadTasks::getInstance().addGenericTask([this, blockPos, uniqueId]() {
            glTextureSubImage2D(mBiomeTexture, 0, blockPos.x, blockPos.y, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &uniqueId);
        });
        return vertex.biomeUniqueId;
    }
    return BiomeUniqueID::INVALID;
}

BiomeUniqueID BiomeGrid::getBiomeIdAtBlockPos(BlockCoord blockPos) const {
    if (blockPos.x < 0 || blockPos.y < 0 || blockPos.x >= mWidthVerts || blockPos.y >= mWidthVerts) [[unlikely]] {
        return BiomeUniqueID::INVALID;
    }

    const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v);
    const BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
    {
        std::shared_lock lock(mPatchMutexes[id]);
        return vertex.biomeUniqueId;
    }
}

bool BiomeGrid::trySpawnLivingBiomeAtBlockPos(BlockCoord blockPos, LivingBiomeType type) {
    assert(IS_SIM_THREAD());
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
        std::lock_guard patchLock(mPatchMutexes[id]);
        vertex.biomeUniqueId = newBiome;
        vertex.distanceFromLivingRoot = 0;
        vertex.livingBiomeId = newId;
        vertex.biomeFlags.clearBit(BiomeFlags::Corruptable);

        // Note the second lock, we must make care to not deadlock
        std::lock_guard biomeLock(mLivingBiomeMutex);
        mLivingBiomes.emplace_back(newId, type, 0, blockPos);
    }
    assert(mBiomeTexture);
    RenderThreadTasks::getInstance().addGenericTask([this, blockPos, newBiome]() {
        glTextureSubImage2D(mBiomeTexture, 0, blockPos.x, blockPos.y, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &newBiome);
    });

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
    mPatchMutexes = std::make_unique<std::shared_mutex[]>(mSpatialGrid.getGridSizeCells());
    mPatchSavesUpToDate = std::make_unique<std::atomic_flag[]>(mSpatialGrid.getGridSizeCells());
    LOG_DEBUG("Biome grid allocated {} mb biome", (mSpatialGrid.getGridSizeCells() * sizeof(BiomePatch)) / 1024.f / 1024.f);
}

bool BiomeGrid::canBlockBeCorruptedInternal(BlockCoord blockPos, LivingBiomeType type) {
    const auto [id, cellOffset] = mSpatialGrid.getIDAndCellOffsetAtPos(blockPos.v);
    const BiomeVertex& vertex = mGrid[id][cellOffset.y * BIOME_PATCH_WIDTH_VERTS + cellOffset.x];
    return vertex.biomeFlags.isBitSet(BiomeFlags::Corruptable);
}
