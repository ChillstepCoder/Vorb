#include "stdafx.h"
#include "CityDebugRenderer.h"

#include "debugging/DebugRenderer.h"

#include "city/City.h"
#include "city/CityBuilder.h"
#include "city/CityPlanner.h"
#include "city/CityQuartermaster.h"
#include "building/buildingBlueprintGenerationContext.h"
#include "item/ItemStockpile.h"

constexpr int DEBUG_ID_CITY = 123;

const int PERIOD_FRAMES = INT32_MAX;

constexpr int MAX_ROOM_COLORS = 8;
constexpr float ROOM_COLOR_ALPHA = 0.2f;
constexpr float ROOM_COLOR_ALPHA_WHITE = 0.6f;
const color4 ROOM_COLORS[MAX_ROOM_COLORS] = {
    color4(1.0f, 0.0f, 0.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 1.0f, 0.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 0.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(1.0f, 1.0f, 0.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 1.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(1.0f, 1.0f, 1.0f, ROOM_COLOR_ALPHA_WHITE),
    color4(1.0f, 0.0f, 1.0f, ROOM_COLOR_ALPHA),
    color4(0.0f, 0.0f, 0.0f, ROOM_COLOR_ALPHA),
};

void CityDebugRenderer::renderBlueprintDebug(BuildingBlueprintGenerationContext& bp, int lifetime, color4* inputColor/* = nullptr*/) {
    PROFILE_FUNCTION();
    //const f32v3 bpRootPos(bp.mTileSpatialGrid.getWorldPos3D());
    //const i32v2& dims = bp.mTileSpatialGrid.getDims2D();
    //constexpr f32 ALPHA = 0.5f;
    //DebugRenderer::drawWireQuad(bpRootPos, f32v2(dims), color4(1.0f, 0.0f, 0.0f, 1.0f), lifetime);

    //// TODO: THIS IS NOT THREAD SAFE
    //{ // Tiles needing items
    //    std::set<TileIndex> tileNeedingItems;
    //    for (auto&& it : bp.tilesNeedingItems) {
    //        for (TileIndex& tileIndex : it.second) {
    //            tileNeedingItems.insert(tileIndex);
    //        }
    //    }

    //    DebugRenderer::reserveFilledQuads(tileNeedingItems.size(), lifetime);
    //    for (TileIndex i : tileNeedingItems) {
    //        const f32v3 worldPos = bp.mTileSpatialGrid.getTileBaseWorldPos3D(i);
    //        // Tiles
    //        switch (bp.tiles[i]) {
    //            case BlueprintTileType::FLOOR:
    //                DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_GRAY_ALPHA(ALPHA), lifetime);
    //                break;
    //            case BlueprintTileType::DOOR:
    //                DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_RED_ALPHA(ALPHA), lifetime);
    //                break;
    //            case BlueprintTileType::WALL:
    //                DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_WHITE_ALPHA(ALPHA), lifetime);
    //                break;
    //            case BlueprintTileType::STAIRS:
    //                DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_CYAN_ALPHA(ALPHA), lifetime);
    //                break;
    //            case BlueprintTileType::STAIRS_FLAT:
    //                DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_BLUE_ALPHA(ALPHA), lifetime);
    //                break;
    //            case BlueprintTileType::NONE:
    //            case BlueprintTileType::AIR:
    //            case BlueprintTileType::TYPES:
    //            default:
    //                assert(false);
    //                break;

    //        }
    //        // Walls
    //        const TileWall southWall = bp.walls.getSouthWallAtTile(i);
    //        const TileWall westWall = bp.walls.getSouthWallAtTile(i);
    //        if (southWall.wallID != TILE_ID_NONE) {
    //            DebugRenderer::drawLine(worldPos, f32v3(1.0f, 0.0f, 0.0f), COLOR_WHITE_ALPHA(ALPHA), lifetime);
    //        }
    //        if (westWall.wallID != TILE_ID_NONE) {
    //            DebugRenderer::drawLine(worldPos, f32v3(0.0f, 1.0f, 0.0f), COLOR_WHITE_ALPHA(ALPHA), lifetime);
    //        }
    //    }
    //}

    //// TODO: NOT THREAD SAFE
    //if (bp.tilesReadyToBuild.size()) { // Tiles ready to build
    //    std::vector<TileIndex> queueCopy = bp.tilesReadyToBuild;
    //    DebugRenderer::reserveFilledQuads(queueCopy.size(), lifetime);
    //    for (size_t i = 0; i < queueCopy.size(); ++i) {
    //        TileIndex tileIndex = queueCopy[i];
    //        DebugRenderer::drawFilledQuad(f32v3(bp.mTileSpatialGrid.getTileBaseWorldPos3D(tileIndex)), f32v2(1.0f), COLOR_GREEN_ALPHA(ALPHA), lifetime);
    //    }
    //}
    assert(false);
}

void CityDebugRenderer::renderCityPlannerDebug(const CityPlanner& cityPlanner) const {

    if (!mNeedsMeshes) {
        return;
    }

    /*if (frameCount <= 0) {
        for (auto&& bp : cityPlanner.mBluePrints) {
            renderBlueprint(*bp);
        }
    }*/
}

void CityDebugRenderer::renderCityBuilderDebug(const CityBuilder& cityBuilder) const {

    if (!mNeedsMeshes) {
        return;
    }

    /*  for (auto&& bp : cityBuilder.mBlueprintsToBuild) {
          renderBlueprintDebug(*bp, 0);
      }*/
}

void CityDebugRenderer::renderCityPlotterDebug(const CityPlotter& cityPlotter) const {
    assert(false);
}

void CityDebugRenderer::renderCityQuartermasterDebug(const CityQuartermaster& cityQuartermaster) const
{
    for (auto&& stockpile : cityQuartermaster.mAllStockpiles) {
        stockpile->renderDebug();
    }
}

void CityDebugRenderer::finishRenderFrame()
{
    if (mNeedsMeshes) {
        mNeedsMeshes = false;
    }
}

void CityDebugRenderer::clearMeshes()
{
    if (!mNeedsMeshes) {
        mNeedsMeshes = true;
        DebugRenderer::clearAllMeshesWithId(DEBUG_ID_CITY);
    }
}
