#include "stdafx.h"
#include "BiomeGrid.h"

BiomeGrid::BiomeGrid(ui32 worldWidthTiles) {
    const ui32 widthVerts = worldWidthTiles / BIOME_VERTEX_STRIDE;
    mSpatialGrid.init(BIOME_VERTEX_STRIDE, widthVerts);
    mTotalVertices = SQ(widthVerts);
    mGrid = std::make_unique<BiomeVertex[]>(mTotalVertices);
}

BiomeGrid::~BiomeGrid() = default;
