#pragma once

#include "util/SpatialGrid2D.h"
#include "definitions/BiomeDef.h"

// TODO: BiomeConstants
constexpr i32 BIOME_VERTEX_STRIDE = 8;

enum class BiomeFlags : ui8 {
    BASE_BIOME = BIT(0),
};

struct BiomeVertex {

    bool isRoot() const {
        return distanceFromRoot == 0;
    }

    ui8 biomeUniqueId = UINT8_MAX;
    BitFlags<BiomeFlags> biomeFlags;
    ui16 distanceFromRoot = 0;
    ui32 rootVertexIndex = 0;
    // A biome vertex owns some number of surrounding tiles. This never changes even if the biome changes.
    // STORE ON TILE INSTEAD. TOO MUCH DATA
    //ui64 ownershipBits[4] = {}; // BL, BR, TL, TR - One bit per tile
};

// Host only?
class BiomeGrid
{
    friend class WorldDataGPUGenerator;
public:
    BiomeGrid(ui32 worldWidthTiles);
    ~BiomeGrid();

    VORB_NON_COPYABLE(BiomeGrid);

    ui32 getTotalVertices() const { return mTotalVertices; }
    ui32 getWidthVertices() const { return mSpatialGrid.getGridWidthCells(); }
    // template <bool THREAD_SAFE>
    const BiomeDef* getBiomeDefAtPoint(f32v2 worldPos) const;

    void setBiomeTexture(VGTexture biomeTexture) { mBiomeTexture = biomeTexture; }
    // Safe to call from render thread
    VGTexture getBiomeTexture() const { return mBiomeTexture; }
private:
    void setVertex(i32v2 vertexPos, BiomeVertex vertex) {
        mGrid[vertexPos.y * getWidthVertices() + vertexPos.x] = vertex;
    }
    std::unique_ptr<BiomeVertex[]> mGrid;
    ui32 mTotalVertices;
    SpatialGrid2D mSpatialGrid;
    // Used by terrain to look up biome info
    VGTexture mBiomeTexture = 0;
};

