#pragma once

#include "tile/TileHarvestable.h"

#include "util/SpatialGrid2D.h"
#include "definitions/BiomeDef.h"
#include "serialization/BitseryExt.h"

// TODO: BiomeConstants
constexpr i32 BIOME_VERTEX_STRIDE = BLOCK_WIDTH;
constexpr int MAX_PRIMARY_RESOURCES_PER_BIOME = 4;

constexpr ui32 BIOME_PATCH_WIDTH_TILES = CHUNK_WIDTH * 2;
constexpr ui32 BIOME_PATCH_WIDTH_VERTS = BIOME_PATCH_WIDTH_TILES / BIOME_VERTEX_STRIDE;
constexpr ui32 BIOME_PATCH_SIZE_VERTS = SQ(BIOME_PATCH_WIDTH_VERTS);

enum class BiomeFlags : ui8 {
    BASE_BIOME = BIT(0),
};

struct BiomeVertex {
    bool isRoot() const {
        return distanceFromRoot == 0;
    }
    bool isWater() const {
        return biomeUniqueId == BiomeUniqueID::Ocean;
    }

    // ======================= Data =======================
    // 
    // approx when used by simulation, made exact by active chunks
    //ui8 biomeResourceAmountsRemaining[MAX_PRIMARY_RESOURCES_PER_BIOME] = {}; // Only 64 tiles per vertex so [0-64]
    ui32 rootVertexIndex = 0;
    ui16 distanceFromRoot = 0;
    BiomeUniqueID biomeUniqueId = BiomeUniqueID::Ocean;
    BitFlags<BiomeFlags> biomeFlags;
    // A biome vertex owns some number of surrounding tiles. This never changes even if the biome changes.
    // STORE ON TILE INSTEAD. TOO MUCH DATA
    //ui64 ownershipBits[4] = {}; // BL, BR, TL, TR - One bit per tile

    // ======================= Serialization =======================
    BINARY_SERIALIZE() {
        s.value1b(biomeUniqueId);
        s.value1b(static_cast<ui8&>(biomeFlags));
        s.value2b(distanceFromRoot);
        s.value4b(rootVertexIndex);
    }
};
static_assert(sizeof(BiomeVertex) == 8, "We are serializing as a binary blob so this must have no automatic padding");

// Host only?
class BiomeGrid
{
    friend class WorldDataGenerator;
public:
    BiomeGrid(ui32 worldWidthTiles);
    ~BiomeGrid();

    VORB_NON_COPYABLE(BiomeGrid);

    ui32 getTotalVertices() const { return mGrid.size(); }
    ui32 getWidthVertices() const { return mSpatialGrid.getGridWidthCells(); }
    // template <bool THREAD_SAFE>
    const BiomeDef* getBiomeDefAtPoint(f32v2 worldPos) const;

    // We will manage the lifetime of the texture
    void setBiomeTexture(VGTexture biomeTexture) { mBiomeTexture = biomeTexture; }
    // Safe to call from render thread
    VGTexture getBiomeTexture() const { return mBiomeTexture; }

    BiomeVertex& getVertexForGeneration(ui32 index) {
        return mGrid[index];
    }

private:
    void initInternal();
    void setVertex(i32v2 vertexPos, BiomeVertex vertex) {
        mGrid[vertexPos.y * getWidthVertices() + vertexPos.x] = vertex;
    }

    // ======================= Data =======================
    std::vector<BiomeVertex> mGrid;
    ui32 mWidthVerts = 0;
    SpatialGrid2D mSpatialGrid;
    // Used by terrain to look up biome info
    VGTexture mBiomeTexture = 0;

    struct BiomePrimaryResourceInfo {
        TileHarvestable type;
        ui8 avgMaxInstancesPerBiomeVertex; //[0-64]
        std::pair<TileID, f32/*probability*/> possibleTiles[4];
    };

    // Stores which primary resources exist in each biome
    TileHarvestable mBiomePrimaryResourcesLookup[MAX_PRIMARY_RESOURCES_PER_BIOME][e_count(BiomeUniqueID)];

    // ======================= Serialization =======================
    BINARY_SERIALIZE() {
        s.value4b(mWidthVerts);
        if (mGrid.empty()) {
            initInternal();
        }
        //s.container(mGrid, mGrid.size());/
        s.ext(mGrid, bitsery::ext::PodStructVector{});
    }
};

