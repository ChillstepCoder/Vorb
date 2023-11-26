#include "stdafx.h"
#include "BiomeGrid.h"

#include "resources/BiomeRepository.h"

BiomeGrid::BiomeGrid(ui32 worldWidthTiles) {
    const ui32 widthVerts = worldWidthTiles / BIOME_VERTEX_STRIDE;
    mSpatialGrid.init(BIOME_VERTEX_STRIDE, widthVerts);
    mTotalVertices = SQ(widthVerts);
    mGrid = std::make_unique<BiomeVertex[]>(mTotalVertices);
}

BiomeGrid::~BiomeGrid() = default;

const BiomeDef* BiomeGrid::getBiomeDefAtPoint(f32v2 position) const {
    const i32v2 blVertex = i32v2(position) / BIOME_VERTEX_STRIDE;
    if (blVertex.x < 0 || blVertex.y < 0 || blVertex.x >= (i32)mSpatialGrid.getGridWidthCells() || blVertex.y >= (i32)mSpatialGrid.getGridWidthCells()) {
        return nullptr;
    }

    ui8 biomeId = mGrid[mSpatialGrid.getIDfromGridXY(blVertex)].biomeID;
    if (biomeId == INVALID_BIOME_ID) return nullptr;
    return &BiomeRepository::get().getLoadedOrUnloadedAsset(biomeId);
}
