#pragma once

#include "tile/TileHarvestable.h"

#include "util/SpatialGrid2D.h"
#include "definitions/BiomeDef.h"
#include "serialization/BitseryExt.h"
#include "world/WorldConstants.h"
#include "BiomeGridEvents.h"


constexpr int MAX_PRIMARY_RESOURCES_PER_BIOME = 4;

enum class BiomeFlags : ui8 {
    Corruptable = BIT(0)
};

struct BiomeVertex {
    bool isCorrupted() const {
        return livingBiomeId != INVALID_LIVING_BIOME_ID;
    }
    bool isWater() const {
        return biomeUniqueId == BiomeUniqueID::Ocean;
    }

    // ======================= Data =======================
    LivingBiomeID livingBiomeId = INVALID_LIVING_BIOME_ID;
    ui16 distanceFromLivingRoot = 0; // Will always be 0 if this is not a living biome
    BiomeUniqueID biomeUniqueId = BiomeUniqueID::Ocean;
    BitFlags<BiomeFlags> biomeFlags;

    // ======================= Serialization =======================
    BINARY_SERIALIZE() {
        s.value1b(biomeUniqueId);
        s.value1b(biomeFlags.getBitsRef());
        s.value2b(distanceFromLivingRoot);
        s.value2b(livingBiomeId);
    }
};
static_assert(sizeof(BiomeVertex) == 6, "We are serializing as a binary blob so this must have no automatic padding");

class LivingBiomeInstance {
public:
    LivingBiomeInstance(LivingBiomeID id, LivingBiomeType type, ui8 powerLevel, BlockCoord rootPos);

    LivingBiomeID mId = INVALID_LIVING_BIOME_ID;
    LivingBiomeType mType = LivingBiomeType::COUNT;
    ui8 mPowerLevel;
    ui32 mSizeBlocks;
    BlockCoord mRootBlockPos;

private:
    friend class BiomeGrid;

    // Sim thread access only, does not need mutex
    std::vector<BlockCoord> mEdgeBlockPositions;
};

typedef std::array<BiomeVertex, BIOME_PATCH_SIZE_VERTS> BiomePatch;
template <typename S>
void serialize(S& s, BiomePatch& p) {
    std::span<BiomeVertex> spn(p.data(), p.size());
    s.ext(spn, bitsery::ext::PodStructSpan{});
}

// Stores biomes and living biomes, and handles biome growth on sim thread
class BiomeGrid {
    friend class WorldSaveContext;
    friend class WorldDataGenerator;
public:
    BiomeGrid(ui32 worldWidthTiles);
    ~BiomeGrid();

    VORB_NON_COPYABLE(BiomeGrid);

    void updateSimThread(TimestampMs simTime);

    ui32 getTotalVertices() const { return mGrid.size() * BIOME_PATCH_SIZE_VERTS; }
    ui32 getWidthVertices() const { return mSpatialGrid.getGridWidthCells() * BIOME_PATCH_WIDTH_VERTS; }
    ui32 getWidthPatches() const { return mSpatialGrid.getGridWidthCells(); }
    std::array<BiomeVertex, BIOME_PATCH_SIZE_VERTS>& getPatch(i32v2 cellXY) {
        return mGrid[mSpatialGrid.getIDfromCellCoords(cellXY)];
    }

    bool canBlockBeCorrupted(BlockCoord blockPos, LivingBiomeType type);

    // Thread safe
    const BiomeDef* getBiomeDefAtPoint(TileCoord worldPos) const;
    // Returns BiomeUniqueID::INVALID on fail
    BiomeUniqueID tryMutateBiomeAtTile(TileCoord worldPos, LivingBiomeType type);
    // Returns BiomeUniqueID::INVALID on fail
    BiomeUniqueID tryMutateBiomeAtBlockPos(BlockCoord blockPos, LivingBiomeType type);

    BiomeUniqueID getBiomeIdAtBlockPos(BlockCoord blockPos) const;

    bool trySpawnLivingBiomeAtBlockPos(BlockCoord blockPos, LivingBiomeType type);

    // We will manage the lifetime of the texture
    void setBiomeTexture(VGTexture biomeTexture) { mBiomeTexture = biomeTexture; }
    // Safe to call from render thread
    VGTexture getBiomeTexture() const { return mBiomeTexture; }

    BiomeVertex& getVertexForGenerationFromBlockPos(BlockCoord blockPos);
    BiomePatch& getPatchForLoad(ui32 patchId) { return mGrid[patchId]; }

    EVENT_LISTENER_FUNCS(BiomeGrid, OnCorruption, BIOME_GRID_EVENT_TYPE::OnCorruption, BiomeGridEvent&);
private:
    void initInternal();
    bool canBlockBeCorruptedInternal(BlockCoord blockPos, LivingBiomeType type);

    // ======================= Data =======================
    std::vector<LivingBiomeInstance> mLivingBiomes;
    std::vector<BiomePatch> mGrid;
    std::unique_ptr<std::shared_mutex[]> mPatchMutexes;
    // NOTE: This is locked while patch mutex is locked
    // To avoid deadlock, this can never be locked before trying to aquire a patch mutex
    // We could run into a race condition with quering a living biome that was just killed,
    // so we must also keep the living biome until every single one of its
    // blocks have been cleared
    mutable std::mutex mLivingBiomeMutex;

    std::unique_ptr<std::atomic_flag[]> mPatchSavesUpToDate;
    ui32 mWidthVerts = 0;
    SpatialGrid2D mSpatialGrid;
    // Used by terrain to look up biome info
    VGTexture mBiomeTexture = 0;

    // Growth
    TimestampMs mNextGrowthTime = 0;
    std::atomic_bool mGrowthInProgress = false;
    RandomGenerator mGrowthGen;
    struct NewGrowth {
        NewGrowth(LivingBiomeID livingId, BiomeUniqueID biomeId, BlockCoord blockId, int edgeIndex) :
            livingId(livingId), biomeId(biomeId), blockCoord(blockCoord), edgeIndex(edgeIndex) {}
        LivingBiomeID livingId;
        BiomeUniqueID biomeId;
        BlockCoord blockCoord;
        int edgeIndex;
    };
    std::vector<NewGrowth> mNewGrowthsBuffer;

    EVENT_DISPATCHER_DEF(BiomeGrid);

    // ======================= Serialization =======================
    BINARY_SERIALIZE() {
        // TODO: This  is  redundant, PodStructVector will dump the size and we have world widthtiles
        s.value4b(mWidthVerts); 
        if (mGrid.empty()) {
            initInternal();
        }
        //s.container(mGrid, mGrid.size());/
        s.ext(mGrid, bitsery::ext::PodStructVector{});
    }
};

