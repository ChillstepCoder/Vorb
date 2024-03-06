#include "stdafx.h"
#include "SettlementRoadNetwork.h"

#include "world/World.h"
#include "world/ownership/OwnershipGrid.h"
#include "world/road/RoadGrid.h"
#include "world/IHeightmapGrid.h"
#include "world/chunk/SimChunkTileGrid.h"

#include "util/MathUtil.hpp"

bool SettlementRoadNetwork::tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos) {
    assert(graph.empty());
    randomGenerator.setSeed(RandomGenerator::DEFAULT_SEED * (ui32)settlement + dTilePos.x ^ dTilePos.y);
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
    constexpr ui32 ROAD_WIDTH_END = 3;
    RoadType roadType = RoadType::Dirt;
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, roadType, 0, dTilePos - DTileCoord(0, -INITIAL_LENGTH), ROAD_WIDTH_START, ROAD_WIDTH_END);
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, roadType, 0, dTilePos - DTileCoord(0, INITIAL_LENGTH), ROAD_WIDTH_START, ROAD_WIDTH_END);
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, roadType, 0, dTilePos - DTileCoord(-INITIAL_LENGTH, 0), ROAD_WIDTH_START, ROAD_WIDTH_END);
    addedCount += (i32)tryAddNewRoadSegment(world, settlement, roadType, 0, dTilePos - DTileCoord(INITIAL_LENGTH, 0), ROAD_WIDTH_START, ROAD_WIDTH_END);

    for (ui32 i = 0; i < 32; ++i) {
        tryExtrudeRandomRoadSegment(world, settlement, roadType, ROAD_WIDTH_END, INITIAL_LENGTH * 0.6f);
    }

    return addedCount > 0;
}

bool SettlementRoadNetwork::tryAddNewRoadSegment(World& world, entt::entity settlement, RoadType roadType, RoadVertexID baseVertexId, DTileCoord targetPos, ui8 baseWidth, ui8 endWidth) {
    RoadVertex& baseVertex = graph[baseVertexId];

    // World bounds
    if (targetPos.x <= 0 || targetPos.y <= 0 || targetPos.x >= world.getWidthDTiles() - 1 || targetPos.y >= world.getWidthDTiles() - 1) [[unlikely]] {
        return false;
    }

    constexpr ui32 POINT_COUNT = 4;
    DTileCoord roadPoints[POINT_COUNT];
    roadPoints[0] = baseVertex.pos;
    roadPoints[POINT_COUNT - 1] = targetPos;
    for (int i = 1; i < POINT_COUNT - 1; ++i) {
        // generate intermediate points
        roadPoints[i] = DTileCoord(i32v2(glm::round(vmath::lerp(f32v2(baseVertex.pos.v), f32v2(targetPos.v), f32(i) / (POINT_COUNT - 1)))));
        roadPoints[i].x += randomGenerator.getRandomIntInRange(-2, 2);
        roadPoints[i].y += randomGenerator.getRandomIntInRange(-2, 2);
    }

    auto getDistanceSqToRoad = [&](DTileCoord pos) -> f32 {
        f32 closestSq = MathUtil::computePointToLineSegmentDistanceSQ(pos.v, roadPoints[0].v, roadPoints[1].v);
        for (int i = 1; i < POINT_COUNT - 1; ++i) {
            f32 distSq = MathUtil::computePointToLineSegmentDistanceSQ(pos.v, roadPoints[i].v, roadPoints[i + 1].v);
            if (distSq < closestSq) {
                closestSq = distSq;
            }
        }
        return closestSq;
    };

    // Check if this vertex has any room for more edges
    ui32 baseVertexNextEdgeIndex = UINT32_MAX;
    for (ui32 i = 0; i < 4; ++i) {
        if (baseVertex.edges[i].first == INVALID_ROAD_EDGE) {
            baseVertexNextEdgeIndex = i;
            break;
        }
    }
    if (baseVertexNextEdgeIndex == UINT32_MAX) {
        return false;
    }

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

            if (heightGrid.getHeightAtVert(pos) <= 0.0f) {
                const f32 distanceSq = getDistanceSqToRoad(pos);
                // If this is too close to road center, cancel the road
                if (distanceSq < MIN_ROAD_DIST) {
                    return false;
                }
                continue;
            }

            RoadPoint point = roadGrid.getRoadPoint<true>(pos);
            if (point.type == 0) {
                roadVertsThisEdge.emplace_back(pos);
                continue;
            }
            // Get distance to line segment and check if its too close.
            // TODO: A road
            if (const DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerData(pos)) {
                // If there is a road owned here
                if (ownerData->owner != entt::null) {
                    
                    const f32 distanceSq = getDistanceSqToRoad(pos);
                    // Road is invalid if it is too close to another road that is not connected to our root vertex
                    if (distanceSq < MIN_ROAD_DIST) {
                        if (ownerData->owner == settlement) {
                            // TODO: Instead of just overlapping every road edge, we should be smarter...
                            //       Roads should merge and stuff
                            if (ownerData->ownerObjectType != DTileOwnerObjectType::RoadEdge) {
                                return false;
                            }
                        }
                        else {
                            return false;
                        }
                    }
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
    newVertex.pos = targetPos;
    newVertex.edges[0] = std::pair<RoadEdgeID, RoadVertexID>(newEdgeID, baseVertexId);
    baseVertex.edges[baseVertexNextEdgeIndex] = std::pair<RoadEdgeID, RoadVertexID>(newEdgeID, newVertexID);
    newEdge.verts[0] = baseVertexId;
    newEdge.verts[1] = newVertexID;
    newEdge.widthTiles[0] = baseWidth;
    newEdge.widthTiles[1] = endWidth;
    newEdge.roadType = roadType;
    newEdge.roadVertsNeedingConstruct = std::move(roadVertsThisEdge);
    newEdge.roadVertsNeedingConstruct.shrink_to_fit();
    newEdge.normalDir = glm::normalize(f32v2(targetPos.v) - f32v2(baseVertex.pos.v));

    // Remove old leaf (if applicable)
    for (size_t i = 0; i < leafVerts.size(); ++i) {
        if (leafVerts[i] == baseVertexId) {
            leafVerts[i] = leafVerts.back();
            leafVerts.pop_back();
            break;
        }
    }

    // Add leaf
    leafVerts.emplace_back(newVertexID);

    // TODO: REMOVE ***DEBUG BUILD ROADS***
    SimChunkTileGrid& tileGrid = world.getSimTileGrid();
    constexpr f32 BLEND_THICKNESS = 1.0f;
    f32 baseWidthf(baseWidth);
    f32 endWidthf(endWidth);
    for (DTileCoord pos : newEdge.roadVertsNeedingConstruct) {
        auto [closestSq, closestT] = MathUtil::computePointToLineSegmentDistanceSQAndT(pos.v, roadPoints[0].v, roadPoints[1].v);
        for (int i = 1; i < POINT_COUNT - 1; ++i) {
            auto [distanceSq, time] = MathUtil::computePointToLineSegmentDistanceSQAndT(pos.v, roadPoints[i].v, roadPoints[i + 1].v);
            if (distanceSq < closestSq) {
                closestSq = distanceSq;
                closestT = time;
            }
        }
        f32 desiredThickness = lerp(baseWidthf, endWidthf, closestT) * 0.5f;
        if (closestSq <= SQ(desiredThickness)) {
            const f32 distance = sqrtf(closestSq);
            const f32 strength = glm::min((desiredThickness - distance) / BLEND_THICKNESS, 1.0f);
            if (roadGrid.setRoadPointIfHigherIntensity(pos, RoadPoint{ .strength = ui8(strength * 255), .type = e_cast(roadType) })) {
                // Clear tile if needed
                TileCoord tCoordsThisDTile[4];
                pos.getCoveredTileCoords(tCoordsThisDTile);
                for (int i = 0; i < 4; ++i) {
                    if (SimTileDataWriteReservationPtr writeLock = tileGrid.tryReserveTileDataAtPosIfNotEmpty(tCoordsThisDTile[i])) {
                        writeLock->reservedCopy.tileId = TILE_ID_NONE;
                        writeLock.reset();
                    }
                }
            }
        
        }
    }
    std::vector<DTileCoord>().swap(newEdge.roadVertsNeedingConstruct);
    return true;
}

bool SettlementRoadNetwork::tryExtrudeRandomRoadSegment(World& world, entt::entity settlement, RoadType roadType, ui8 width, f32 length) {
    // We can no longer expand?
    if (leafVerts.size() == 0) {
        return false;
    }
    ui32 leafIndex = randomGenerator.getRandomUIntInRange(0, leafVerts.size());
    const RoadVertexID leafVertexId = leafVerts[leafIndex];
    const RoadVertex& leafVertex = graph[leafVertexId];
    const RoadEdgeDetails prevEdge = edgeDetails[leafVertex.edges[0].first];
    const bool trySplit = randomGenerator.getRandomBool();
    if (trySplit) {
        const int splitType = randomGenerator.getRandomUIntInRange(0, 4);
        switch (splitType) {
            case 0: {
                // Fork
                const f32 bendAngleRad = randomGenerator.getRandomFloatUnsigned() * M_PI_4F + M_PI_4F;
                const f32v2 newNormalA = MathUtil::rotateVector2DRad(prevEdge.normalDir, bendAngleRad);
                const f32v2 newNormalB = MathUtil::rotateVector2DRad(prevEdge.normalDir, -bendAngleRad);
                int addedCount = 0;
                addedCount += tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(newNormalA * length)), prevEdge.widthTiles[1], width);
                addedCount += tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(newNormalB * length)), prevEdge.widthTiles[1], width);
                if (addedCount) {
                    return true;
                }
                break;
            }
            case 1: {
                // 4 way
                const f32 bendAngleRad = M_PI_2F;
                const f32v2 newNormalA = MathUtil::rotateVector2DRad(prevEdge.normalDir, bendAngleRad);
                const f32v2 newNormalB = MathUtil::rotateVector2DRad(prevEdge.normalDir, -bendAngleRad);
                int addedCount = 0;
                addedCount += tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(prevEdge.normalDir * length)), prevEdge.widthTiles[1], width);
                addedCount += tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(newNormalA * length)), prevEdge.widthTiles[1], width);
                addedCount += tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(newNormalB * length)), prevEdge.widthTiles[1], width);
                if (addedCount) {
                    return true;
                }
                break;
            }
            case 2: {
                // Left T Junction
                const f32 bendAngleRad = M_PI_2F;
                const f32v2 newNormalA = MathUtil::rotateVector2DRad(prevEdge.normalDir, bendAngleRad);
                int addedCount = 0;
                addedCount += tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(prevEdge.normalDir * length)), prevEdge.widthTiles[1], width);
                addedCount += tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(newNormalA * length)), prevEdge.widthTiles[1], width);
                if (addedCount) {
                    return true;
                }
                break;
            }
            case 3: {
                // Right T Junction
                const f32 bendAngleRad = M_PI_2F;
                const f32v2 newNormalA = MathUtil::rotateVector2DRad(prevEdge.normalDir, bendAngleRad);
                int addedCount = 0;
                addedCount += tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(prevEdge.normalDir * length)), prevEdge.widthTiles[1], width);
                addedCount += tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(newNormalA * length)), prevEdge.widthTiles[1], width);
                if (addedCount) {
                    return true;
                }
                break;
            }
            default:
                assert(false);
        }
    }

    // Random bend angle
    const f32 bendAngleRad = randomGenerator.getRandomFloatSigned() * M_PI_2F;
    const f32v2 newNormal = MathUtil::rotateVector2DRad(prevEdge.normalDir, bendAngleRad);
    return tryAddNewRoadSegment(world, settlement, roadType, leafVertexId, leafVertex.pos + DTileCoord(i32v2(newNormal * length)), prevEdge.widthTiles[1], width);
}

bool SettlementLayoutManager::tryAddSector(DTileCoord center, f32 desiredRadius) {
    if (mSectors.empty()) [[unlikely]] {
        mSectors.emplace_back(center, desiredRadius);
        return true;
    }
    SettlementSectorID closestSector = INVALID_SECTOR_ID;
    f32 closestDistSq = FLT_MAX;
    for (size_t i = 0; i < mSectors.size(); ++i) {
        const SettlementSector& sector = mSectors[i];
        f32 distSq = glm::distance2(sector.center.v, center.v);
        if (distSq < SQ(sector.desiredRadius + desiredRadius)) {
             return false;
        }
        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closestSector = i;
        }
    }

    SettlementSector newSector(center, desiredRadius);

    // Create a road between new sector and closest other sector
   
    mSectors.emplace_back(center, desiredRadius);
    return true;
}

//https://stackoverflow.com/questions/14307158/how-do-you-check-for-intersection-between-a-line-segment-and-a-line-ray-emanatin
std::optional<f32> GetRayToLineSegmentIntersection(f32v2 rayOrigin, f32v2 rayDirection, f32v2 point1, f32v2 point2)
{
    f32v2 v1 = rayOrigin - point1;
    f32v2 v2 = point2 - point1;
    f32v2 v3 = f32v2(-rayDirection.y, rayDirection.x);


    f32 dot = glm::dot(v2, v3);
    if (abs(dot) < 0.000001)
        return std::nullopt;

    f32 t1 = MathUtil::crossProduct2d(v2, v1) / dot;
    f32 t2 = glm::dot(v1, v3) / dot;

    if (t1 >= 0.0 && (t2 >= 0.0 && t2 <= 1.0))
        return t1;

    return std::nullopt;
}

std::optional<float> intersectInfiniteLineWithSegment(f32v2 p, f32v2 normal, f32v2 s[2]) {
    f32v2 d = s[1] - s[0]; // Direction vector of the segment
    float det = glm::cross(normal, d); // Determinant for checking parallel lines

    if (det == 0) return std::nullopt; // Lines are parallel, no intersection

    f32v2 diff = p - s[0];
    float u = glm::cross(diff, normal) / det;
    float t = glm::cross(diff, d) / det;

    if (u >= 0 && u <= 1) return t; // Intersection within the segment bounds

    return std::nullopt; // No valid intersection
}

std::optional<RoadSegmentHitResult> SettlementRoadNetworkNew::traceAgainstRoadSegments(DTileCoord start, DTileCoord end) {
    for (RoadSegmentID id = 0; id < roadSegments.size(); ++id) {
        RoadSegment& segment = roadSegments[id];
        if (MathUtil::)
    }
}
