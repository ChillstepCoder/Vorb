#include "stdafx.h"
#include "SettlementRoadNetwork.h"

#include "world/World.h"
#include "world/ownership/OwnershipGrid.h"
#include "world/road/RoadGrid.h"
#include "world/IHeightmapGrid.h"

#include "util/MathUtil.hpp"

bool SettlementRoadNetwork::tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos) {
    assert(graph.empty());
    graph.reserve(64);
    edgeDetails.reserve(64);

    constexpr i32 INITIAL_LENGTH = 32;
    RoadGrid& roadGrid = world.getRoadGrid();
    OwnershipGrid& ownerGrid = world.getOwnershipGrid();
    // ROAD_POINT_STRIDE
    roadGrid.setRoadPoint(dTilePos, RoadPoint{ .strength = 255, .type = 0 });
    RoadVertex vertex;
    vertex.pos = dTilePos;
    graph.emplace_back(std::move(vertex));

    DTileCoord a(i32v2(45));
    DTileCoord b(i32v2(55));
    DTileCoord c = a + b;

    i32 addedCount = 0;
    constexpr ui32 ROAD_WIDTH_START = 5;
    constexpr ui32 ROAD_WIDTH_END = 1;
    RoadType roadType = RoadType::Dirt;
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, roadType, 0, dTilePos - DTileCoord(INITIAL_LENGTH, -INITIAL_LENGTH), ROAD_WIDTH_START, ROAD_WIDTH_END);
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, roadType, 0, dTilePos - DTileCoord(INITIAL_LENGTH, INITIAL_LENGTH), ROAD_WIDTH_START, ROAD_WIDTH_END);
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, roadType, 0, dTilePos - DTileCoord(-INITIAL_LENGTH, -INITIAL_LENGTH), ROAD_WIDTH_START, ROAD_WIDTH_END);
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, roadType, 0, dTilePos - DTileCoord(-INITIAL_LENGTH, INITIAL_LENGTH), ROAD_WIDTH_START, ROAD_WIDTH_END);

    return addedCount > 0;
}

bool SettlementRoadNetwork::tryAddNewRoadSegment(World& world, entt::entity settlement, RoadType roadType, RoadVertexID baseVertexId, DTileCoord targetPos, ui8 baseWidth, ui8 endWidth) {
    RoadVertex& baseVertex = graph[baseVertexId];
    i32 aabbPadding = (i32)glm::max(baseWidth, endWidth);
    DTileCoord offset = targetPos - baseVertex.pos;
    i32AABB2 aabb;
    DTileCoord xSpan;
    DTileCoord ySpan;
    if (baseVertex.pos.x < targetPos.x) {
        xSpan.x = baseVertex.pos.x - aabbPadding;
        xSpan.y = targetPos.x + aabbPadding;
    }
    else {
        xSpan.x = targetPos.x - aabbPadding;
        xSpan.y = baseVertex.pos.x + aabbPadding;
    }
    if (baseVertex.pos.y < targetPos.y) {
        ySpan.x = baseVertex.pos.y - aabbPadding;
        ySpan.y = targetPos.y + aabbPadding;
    }
    else {
        ySpan.x = targetPos.y - aabbPadding;
        ySpan.y = baseVertex.pos.y + aabbPadding;
    }

    aabb.pos = i32v2(xSpan.x, ySpan.x);
    aabb.dims = i32v2(xSpan.y - xSpan.x, ySpan.y - ySpan.x);

    // Loop through the AABB and check for if it is owned already
    OwnershipGrid& ownerGrid = world.getOwnershipGrid();
    IHeightmapGrid& heightGrid = world.getHeightmapGrid();
    RoadGrid& roadGrid = world.getRoadGrid();

    i32v2 maxCoord = aabb.pos + aabb.dims;

    std::vector<DTileCoord> roadVertsThisEdge;
    roadVertsThisEdge.reserve(128);

    constexpr f32 MIN_ROAD_DIST = SQ(1.5f);
    DTileCoord pos;
    for (pos.y = aabb.pos.y; pos.y < maxCoord.y; ++pos.y) {
        for (pos.x = aabb.pos.x; pos.x < maxCoord.x; ++pos.x) {
            RoadPoint point = roadGrid.getRoadPoint<true>(pos);
            if (point.type == 0) {
                roadVertsThisEdge.emplace_back(pos);
                continue;
            }
            // Get distance to line segment and check if its too close.
            // TODO: A road
            if (const DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerData(pos)) {
                // If there is a road owned by someone else here
                if (ownerData->owner != entt::null && ownerData->owner != settlement) {
                    const f32 distanceSq = MathUtil::computePointToLineSegmentDistanceSQ(pos.v, baseVertex.pos.v, targetPos.v);
                    // Road is invalid if it is too close to another road
                    if (distanceSq < MIN_ROAD_DIST) {
                        return false;
                    }
                }
                else if (heightGrid.getHeightAtVert(pos) <= 0.0f) {
                    return false;
                }
                else {
                    roadVertsThisEdge.emplace_back(pos);
                }
            }
        }
    }

    // Create new vertex and edge
    RoadEdgeID newEdgeID = edgeDetails.size();
    RoadEdgeDetails& newEdge = edgeDetails.emplace_back();
    RoadVertexID newVertexID = graph.size();
    RoadVertex& newVertex = graph.emplace_back();
    newVertex.edges[0] = std::pair<RoadEdgeID, RoadVertexID>(newEdgeID, baseVertexId);
    newEdge.verts[0] = baseVertexId;
    newEdge.verts[1] = newVertexID;
    newEdge.widthTiles[0] = baseWidth;
    newEdge.widthTiles[1] = endWidth;
    newEdge.roadType = roadType;
    newEdge.roadVertsNeedingConstruct = std::move(roadVertsThisEdge);
    newEdge.roadVertsNeedingConstruct.shrink_to_fit();

    bool addedRoadVertex = false;
    for (ui32 i = 0; i < 4; ++i) {
        if (baseVertex.edges[i].first == INVALID_ROAD_EDGE) {
            addedRoadVertex = true;
            baseVertex.edges[i] = std::pair<RoadEdgeID, RoadVertexID>(newEdgeID, newVertexID);
            break;
        }
    }
    assert(addedRoadVertex);

    // TODO: REMOVE ***DEBUG BUILD ROADS***
    constexpr f32 BLEND_THICKNESS = 1.0f;
    f32 baseWidthf(baseWidth);
    f32 endWidthf(endWidth);
    for (DTileCoord pos : newEdge.roadVertsNeedingConstruct) {
        auto [distanceSq, time] = MathUtil::computePointToLineSegmentDistanceSQAndT(pos.v, baseVertex.pos.v, targetPos.v);
        f32 desiredThickness = lerp(baseWidthf, endWidthf, time) * 0.5f;
        if (distanceSq <= SQ(desiredThickness)) {
            const f32 distance = sqrtf(distanceSq);
            const f32 strength = glm::min((desiredThickness - distance) / BLEND_THICKNESS, 1.0f);
            roadGrid.setRoadPointIfHigherIntensity(pos, RoadPoint{ .strength = ui8(strength * 255), .type = e_cast(roadType) });
        }
    }
    std::vector<DTileCoord>().swap(newEdge.roadVertsNeedingConstruct);
    return true;
}
