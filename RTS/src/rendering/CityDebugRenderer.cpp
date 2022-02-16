#include "stdafx.h"
#include "CityDebugRenderer.h"

#include "DebugRenderer.h"

#include "box2d/b2_collision.h"

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

void CityDebugRenderer::renderBlueprintDebug(BuildingBlueprint& bp, color4* inputColor/* = nullptr*/) {

    if (bp.isGenerating)
        return;

    // Render the AABB of the floor plan
    b2AABB aabb;
    aabb.lowerBound.x = bp.aabb.pos.x;
    aabb.lowerBound.y = bp.aabb.pos.y;
    
    aabb.upperBound.x = aabb.lowerBound.x + bp.aabb.dims.x;
    aabb.upperBound.y = aabb.lowerBound.y + bp.aabb.dims.y;

    constexpr f32 EPSILON = 0.001f;
    const f32 height = 1.0f;
    const f32 heightPlusE = height + EPSILON;

    // Render all the metadata on bottom
    for (int y = 0; y < bp.aabb.dims.y; ++y) {
        for (int x = 0; x < bp.aabb.dims.x; ++x) {
            const int index = y * bp.aabb.dims.x + x;
            RoomNodeID id = bp.ownerArray[index];
            if (id != INVALID_ROOM_ID) {
                const ui32v2 worldPos = bp.aabb.pos + ui32v2(x, y);
                const color4& color = inputColor ? *inputColor : ROOM_COLORS[id % MAX_ROOM_COLORS];
                DebugRenderer::drawFilledQuad(f32v3((f32)worldPos.x, (f32)worldPos.y, height), f32v2(1.0f), color4(color.r, color.g, color.b, 128u));
            }
        }
    }

    DebugRenderer::drawAABB(aabb, heightPlusE, inputColor ? *inputColor : color4(0.7f, 0.4f, 0.0f));
    // Render the room graph in world space
    int i = 0;
    for (auto&& node : bp.rooms) {

        const color4& color = ROOM_COLORS[i % MAX_ROOM_COLORS];
        DebugRenderer::drawAABB(node.aabb, heightPlusE, color4(color.r, color.g, color.b, 255u));
        // Draw parent line
        if (node.parentRoom != INVALID_ROOM_ID) {
            RoomNode& parent = bp.rooms[node.parentRoom];
            const f32v3 startPos(node.aabb.getCenter().x + 0.5f, node.aabb.getCenter().y + 0.5f, heightPlusE);
            const f32v3 endPos(bp.aabb.pos.x + parent.offsetFromZero.x + 0.5f, bp.aabb.pos.y + parent.offsetFromZero.y + 0.5f, heightPlusE);
            if (node.isPrivate) {
                DebugRenderer::drawLine(startPos, endPos - startPos, color4(1.0f, 1.0f, 0.0f));
            }
            else {
                DebugRenderer::drawLine(startPos, endPos - startPos, color4(0.0f, 1.0f, 0.0f));
            }
        }
        ++i;
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

    for (auto&& bp : cityBuilder.mBlueprintsToBuild) {
        renderBlueprintDebug(*bp);
    }
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
        if (plot.neighborRoads[e_cast(Cartesian::LEFT)] != INVALID_ROAD_ID) {
            DebugRenderer::drawLine(f32v2(plot.aabb.pos), f32v2(0.0f, plot.aabb.dims.y), color4(0.0f, 1.0f, 0.0f), PERIOD_FRAMES, DEBUG_ID_CITY);
        }
        if (plot.neighborRoads[e_cast(Cartesian::RIGHT)] != INVALID_ROAD_ID) {
            DebugRenderer::drawLine(f32v2(plot.aabb.pos.x + plot.aabb.dims.x, plot.aabb.pos.y), f32v2(0.0f, plot.aabb.dims.y), color4(0.0f, 1.0f, 0.0f), PERIOD_FRAMES, DEBUG_ID_CITY);
        }
        if (plot.neighborRoads[e_cast(Cartesian::DOWN)] != INVALID_ROAD_ID) {
            DebugRenderer::drawLine(f32v2(plot.aabb.pos.x, plot.aabb.pos.y), f32v2(plot.aabb.dims.x, 0.0f), color4(0.0f, 1.0f, 0.0f), PERIOD_FRAMES, DEBUG_ID_CITY);
        }
        if (plot.neighborRoads[e_cast(Cartesian::UP)] != INVALID_ROAD_ID) {
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
