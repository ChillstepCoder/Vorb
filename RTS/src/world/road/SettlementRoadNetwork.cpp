#include "stdafx.h"
#include "SettlementRoadNetwork.h"

#include "world/World.h"
#include "world/ownership/OwnershipGrid.h"
#include "world/road/RoadGrid.h"


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
    constexpr ui32 ROAD_WIDTH = 3;
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, 0, dTilePos - DTileCoord(0, -INITIAL_LENGTH), ROAD_WIDTH, ROAD_WIDTH);
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, 0, dTilePos - DTileCoord(-INITIAL_LENGTH, 0), ROAD_WIDTH, ROAD_WIDTH);
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, 0, dTilePos - DTileCoord(INITIAL_LENGTH, 0), ROAD_WIDTH, ROAD_WIDTH);
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, 0, dTilePos - DTileCoord(0, INITIAL_LENGTH), ROAD_WIDTH, ROAD_WIDTH);

    return addedCount > 0;
}

bool SettlementRoadNetwork::tryAddNewRoadSegment(World& world, entt::entity settlement, RoadVertexID baseVertexId, DTileCoord targetPos, ui8 baseWidth, ui8 endWidth) {
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

    // Loop through the AABB 

    return true;
}
