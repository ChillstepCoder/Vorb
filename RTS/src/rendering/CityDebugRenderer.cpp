#include "stdafx.h"
#include "CityDebugRenderer.h"

#include "debugging/DebugRenderer.h"

#include "city/City.h"
#include "city/CityBuilder.h"
#include "city/CityPlanner.h"
#include "city/CityPlotter.h"
#include "city/CityQuartermaster.h"
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

void CityDebugRenderer::renderBlueprintDebug(BuildingBlueprint& bp, int lifetime, color4* inputColor/* = nullptr*/) {
    PROFILE_FUNCTION();
    const f32v3 bpRootPos(bp.mTileSpatialGrid.getWorldPos3D());
    const i32v2& dims = bp.mTileSpatialGrid.getDims2D();
    constexpr f32 ALPHA = 0.5f;
    DebugRenderer::drawWireQuad(bpRootPos, f32v2(dims), color4(1.0f, 0.0f, 0.0f, 1.0f), lifetime);

    // TODO: THIS IS NOT THREAD SAFE
    { // Tiles needing items
        std::set<TileIndex> tileNeedingItems;
        for (auto&& it : bp.tilesNeedingItems) {
            for (TileIndex& tileIndex : it.second) {
                tileNeedingItems.insert(tileIndex);
            }
        }

        DebugRenderer::reserveFilledQuads(tileNeedingItems.size(), lifetime);
        for (TileIndex i : tileNeedingItems) {
            const f32v3 worldPos = bp.mTileSpatialGrid.getTileBaseWorldPos3D(i);
            // Tiles
            switch (bp.tiles[i]) {
                case BlueprintTileType::FLOOR:
                    DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_GRAY_ALPHA(ALPHA), lifetime);
                    break;
                case BlueprintTileType::DOOR:
                    DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_RED_ALPHA(ALPHA), lifetime);
                    break;
                case BlueprintTileType::WALL:
                    DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_WHITE_ALPHA(ALPHA), lifetime);
                    break;
                case BlueprintTileType::STAIRS:
                    DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_CYAN_ALPHA(ALPHA), lifetime);
                    break;
                case BlueprintTileType::STAIRS_FLAT:
                    DebugRenderer::drawFilledQuad(worldPos, f32v2(1.0f), COLOR_BLUE_ALPHA(ALPHA), lifetime);
                    break;
                case BlueprintTileType::NONE:
                case BlueprintTileType::AIR:
                case BlueprintTileType::TYPES:
                default:
                    assert(false);
                    break;

            }
            // Walls
            const TileWall southWall = bp.walls.getSouthWallAtTile(i);
            const TileWall westWall = bp.walls.getSouthWallAtTile(i);
            if (southWall.wallID != TILE_ID_NONE) {
                DebugRenderer::drawLine(worldPos, f32v3(1.0f, 0.0f, 0.0f), COLOR_WHITE_ALPHA(ALPHA), lifetime);
            }
            if (westWall.wallID != TILE_ID_NONE) {
                DebugRenderer::drawLine(worldPos, f32v3(0.0f, 1.0f, 0.0f), COLOR_WHITE_ALPHA(ALPHA), lifetime);
            }
        }
    }

    // TODO: NOT THREAD SAFE
    if (bp.tilesReadyToBuild.size()) { // Tiles ready to build
        std::vector<TileIndex> queueCopy = bp.tilesReadyToBuild;
        DebugRenderer::reserveFilledQuads(queueCopy.size(), lifetime);
        for (size_t i = 0; i < queueCopy.size(); ++i) {
            TileIndex tileIndex = queueCopy[i];
            DebugRenderer::drawFilledQuad(f32v3(bp.mTileSpatialGrid.getTileBaseWorldPos3D(tileIndex)), f32v2(1.0f), COLOR_GREEN_ALPHA(ALPHA), lifetime);
        }
    }
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

    if (!mNeedsMeshes) {
        return;
    }

    // Render districts
    for (size_t i = 0; i < cityPlotter.mDistricts.size(); ++i) {
        const CityDistrict& district = *cityPlotter.mDistricts[i];
        color4 color;
        switch (district.type) {
            case DistrictType::Rural:
                color = color4(0.0f, 1.0f, 0.0f, ROOM_COLOR_ALPHA);
                break;
            case DistrictType::Farming:
                color = color4(0.0f, 0.6f, 0.2f, ROOM_COLOR_ALPHA);
                break;
            case DistrictType::Residential:
                color = color4(1.0f, 0.5f, 0.0f, ROOM_COLOR_ALPHA);
                break;
            case DistrictType::Commercial:
                color = color4(1.0f, 1.0f, 0.0f, ROOM_COLOR_ALPHA);
                break;
            case DistrictType::Government:
                color = color4(0.7f, 0.0f, 0.7f, ROOM_COLOR_ALPHA);
                break;
            case DistrictType::Military:
                color = color4(1.0f, 0.0f, 0.0f, ROOM_COLOR_ALPHA);
                break;
            case DistrictType::Industrial:
                color = color4(0.8f, 0.8f, 0.8f, ROOM_COLOR_ALPHA);
                break;
            default:
                color = color4(1.0f, 1.0f, 1.0f, ROOM_COLOR_ALPHA);
        };
        DebugRenderer::drawFilledQuad(f32v2(district.aabb.pos), f32v2(district.aabb.dims), color, PERIOD_FRAMES, DEBUG_ID_CITY);
    }

    // Render plots
    for (size_t i = 0; i < cityPlotter.mPlots.size(); ++i) {
        const auto& plot = *cityPlotter.mPlots[i];
        color4 color;
        color = color4(1.0f, 0.0f, 1.0f, ROOM_COLOR_ALPHA * 2);
        DebugRenderer::drawAABB(f32v2(plot.aabb.pos), f32v2(plot.aabb.dims), 0.0f, color, PERIOD_FRAMES, DEBUG_ID_CITY);
        // Plot edges
        if (plot.neighborRoads[e_cast(Cartesian::WEST)] != INVALID_ROAD_ID) {
            DebugRenderer::drawLine(f32v2(plot.aabb.pos), f32v2(0.0f, plot.aabb.dims.y), color4(0.0f, 1.0f, 0.0f), PERIOD_FRAMES, DEBUG_ID_CITY);
        }
        if (plot.neighborRoads[e_cast(Cartesian::EAST)] != INVALID_ROAD_ID) {
            DebugRenderer::drawLine(f32v2(plot.aabb.pos.x + plot.aabb.dims.x, plot.aabb.pos.y), f32v2(0.0f, plot.aabb.dims.y), color4(0.0f, 1.0f, 0.0f), PERIOD_FRAMES, DEBUG_ID_CITY);
        }
        if (plot.neighborRoads[e_cast(Cartesian::SOUTH)] != INVALID_ROAD_ID) {
            DebugRenderer::drawLine(f32v2(plot.aabb.pos.x, plot.aabb.pos.y), f32v2(plot.aabb.dims.x, 0.0f), color4(0.0f, 1.0f, 0.0f), PERIOD_FRAMES, DEBUG_ID_CITY);
        }
        if (plot.neighborRoads[e_cast(Cartesian::NORTH)] != INVALID_ROAD_ID) {
            DebugRenderer::drawLine(f32v2(plot.aabb.pos.x, plot.aabb.pos.y + plot.aabb.dims.y), f32v2(plot.aabb.dims.x, 0.0f), color4(0.0f, 1.0f, 0.0f), PERIOD_FRAMES, DEBUG_ID_CITY);
        }
    }

    // Render roads, starting with root road and traversing
    if (cityPlotter.mCity.mRoads.size()) {
        std::set<CityRoad*> visitedRoads;
        std::queue<CityRoad*> roadsToVisit;
        int i = 0;
        CityRoad* road = cityPlotter.mCity.mRoads[i].get();
        visitedRoads.insert(road);
        color4 roadColor = color4(1.0f, 1.0f, 0.0f);
        while (road) {

            DebugRenderer::drawCircle(f32v3(road->startPos.x, road->startPos.y, 0.0f) + f32v3(0.5f, 0.5f, 0.0f), road->width * 0.5f, roadColor, PERIOD_FRAMES, DEBUG_ID_CITY);
            DebugRenderer::drawCircle(f32v3(road->endPos.x, road->endPos.y, 0.0f) + f32v3(0.5f, 0.5f, 0.0f), road->width * 0.5f, roadColor, PERIOD_FRAMES, DEBUG_ID_CITY);
            DebugRenderer::drawLineBetweenPoints(f32v2(road->startPos) + f32v2(0.5f), f32v2(road->endPos) + f32v2(0.5f), roadColor, PERIOD_FRAMES, DEBUG_ID_CITY);

            for (size_t j = 0; j < road->neighborRoads.size(); ++j) {
                CityRoad* neighbor = road->neighborRoads[j].second;
                if (visitedRoads.find(neighbor) == visitedRoads.end()) {
                    roadsToVisit.push(neighbor);
                    visitedRoads.insert(neighbor);
                }
            }

            if (roadsToVisit.size()) {
                road = roadsToVisit.front();
                roadsToVisit.pop();
            }
            else {
                road = nullptr;
                for (++i; i < cityPlotter.mCity.mRoads.size(); ++i) {
                    CityRoad* r = cityPlotter.mCity.mRoads[i].get();
                    if (visitedRoads.find(r) == visitedRoads.end()) {
                        road = r;
                        visitedRoads.insert(road);
                        assert(road->neighborRoads.size() == 0);
                        roadColor = color4(1.0f, 0.0f, 0.0f);
                        break;
                    }
                }
            }
        }
    }
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
