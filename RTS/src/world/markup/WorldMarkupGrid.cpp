#include "stdafx.h"
#include "WorldMarkupGrid.h"

WorldMarkupGrid::WorldMarkupGrid(ui32 worldWidthTiles)
{
    const ui32 widthVerts = worldWidthTiles / MARKUP_VERTEX_STRIDE;
    mWidthChunks = worldWidthTiles / CHUNK_WIDTH;
    mSpatialGrid.init(MARKUP_VERTEX_STRIDE, widthVerts);
    mTotalVertices = SQ(widthVerts);
    mMarkup = std::make_unique<WorldMarkupData[]>(mTotalVertices);
    mChunkMarkup = std::make_unique<WorldChunkMarkupData[]>(SQ(mWidthChunks));
    LOG_DEBUG("Markup grid allocated {} mb markup",
        (mTotalVertices * sizeof(WorldMarkupData) + sizeof(WorldChunkMarkupData) * SQ(mWidthChunks)) / 1024.f / 1024.f);
}

WorldMarkupGrid::~WorldMarkupGrid() = default;

const WorldMarkupData* WorldMarkupGrid::getMarkupAtPoint(f32v2 worldPos) const {
    const i32v2 blVertex = i32v2(worldPos) / MARKUP_VERTEX_STRIDE;
    if (blVertex.x < 0 || blVertex.y < 0 || blVertex.x >= (i32)mSpatialGrid.getGridWidthCells() || blVertex.y >= (i32)mSpatialGrid.getGridWidthCells()) {
        return nullptr;
    }
    return &mMarkup[mSpatialGrid.getIDfromGridXY(blVertex)];
}

const WorldChunkMarkupData* WorldMarkupGrid::getChunkMarkupAtPoint(f32v2 worldPos) const {
    worldPos /= CHUNK_WIDTH;
    i32v2 chunkPos = i32v2(worldPos);
    if (chunkPos.x < 0 || chunkPos.y < 0 || chunkPos.x >= mWidthChunks || chunkPos.y >= mWidthChunks) [[unlikely]] {
        return nullptr;
    }
    return &mChunkMarkup[chunkPos.y * mWidthChunks + chunkPos.x];
}

const WorldBodyMarkupData* WorldMarkupGrid::getBodyDataAtPoint(f32v2 worldPos) const {
    const WorldMarkupData* baseMarkup = getMarkupAtPoint(worldPos);
    if (!baseMarkup) return nullptr;
    if (baseMarkup->bodyIndex == UINT32_MAX) return nullptr;
    return &mBodies[baseMarkup->bodyIndex];
}
