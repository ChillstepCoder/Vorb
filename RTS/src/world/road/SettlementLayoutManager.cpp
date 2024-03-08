#include "stdafx.h"
#include "SettlementLayoutManager.h"

#include "world/World.h"
#include "world/ownership/OwnershipGrid.h"
#include "world/road/RoadGrid.h"
#include "world/IHeightmapGrid.h"
#include "world/chunk/SimChunkTileGrid.h"

#include "util/IntersectionHit.h"
#include "util/IntersectionUtil.h"
#include "util/MathUtil.hpp"

#include "debugging/DebugRenderer.h"

bool SettlementLayoutManager::tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos) {
    mWorld = &world;
    mRootPos = dTilePos;
    assert(mSectors.empty());
    mRoadNetwork.randomGenerator.setSeed(RandomGenerator::DEFAULT_SEED * (ui32)settlement + dTilePos.x ^ dTilePos.y);
    mSectors.reserve(64);
    mOpenSectors.reserve(64);
    mRoadNetwork.roadSegments.reserve(64);

    constexpr i32 INITIAL_LENGTH = 32;
    RoadGrid& roadGrid = world.getRoadGrid();
    OwnershipGrid& ownerGrid = world.getOwnershipGrid();

    DTileCoord a(i32v2(45));
    DTileCoord b(i32v2(55));
    DTileCoord c = a + b;
    
    constexpr f32 DESIRED_RADIUS = 16.f;
    constexpr f32 PADDED_RADIUS = DESIRED_RADIUS + 1.0f;
    ui32 addedCount = 0;

    addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS, (i32)-PADDED_RADIUS), DESIRED_RADIUS);
    addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS, (i32)PADDED_RADIUS), DESIRED_RADIUS);
    addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)-PADDED_RADIUS), DESIRED_RADIUS);
    addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)PADDED_RADIUS), DESIRED_RADIUS);
    addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)PADDED_RADIUS * 3.0f), DESIRED_RADIUS);
   
    return addedCount > 1;
}

bool SettlementLayoutManager::tryAddSector(entt::entity settlement, DTileCoord center, f32 desiredRadius) {

    if (mSectors.empty()) [[unlikely]] {
        mSectors.emplace_back(center, desiredRadius, mSectors.size());
        return true;
    }

    // Sort all sectors
    boost::container::flat_multimap<f32 /*distSq*/, SettlementSectorID> sectorDistances;
    sectorDistances.reserve(mSectors.size());
    for (SettlementSectorID i = 0; i < mSectors.size(); ++i) {
        const SettlementSector& sector = mSectors[i];
        const f32 distSq = glm::distance2(f32v2(sector.center.v), f32v2(center.v));
        if (distSq < SQ(sector.desiredRadius + desiredRadius)) {
             return false;
        }
        sectorDistances.emplace(distSq, i);
    }

    SettlementSector newSector(center, desiredRadius, mSectors.size());

    constexpr ui32 DESIRED_ROAD_WIDTH = 3;

    // Create a road between new sector and closest other sectors
    auto it = sectorDistances.begin();
    constexpr ui32 MAX_ROADS_ADDED = 3;
    bool didAddRoad = false;
    for (ui32 i = 0; i < MAX_ROADS_ADDED && it != sectorDistances.end(); ++i, ++it) {
        const SettlementSector& otherSector = mSectors[it->second];
        if (!mRoadNetwork.simpleTraceAgainstSolidRoadSegments(newSector.center, otherSector.center)) {
            // Clear line of sight to other sector, now try making a road
            const f32 distanceRatio = newSector.desiredRadius / (newSector.desiredRadius + otherSector.desiredRadius);
            didAddRoad |= mRoadNetwork.tryAddRoadBetweenSectorPoints(
                *mWorld,
                settlement,
                newSector.center,
                otherSector.center,
                DTileCoord(i32v2(glm::round(vmath::lerp(f32v2(newSector.center.v), f32v2(otherSector.center.v), distanceRatio)))),
                RoadType::Dirt,
                DESIRED_ROAD_WIDTH
            );
        }
    }

    // Sector must have added one road or it is invalid
    if (!didAddRoad) {
        return false;
    }
   
    mSectors.emplace_back(std::move(newSector));
    return true;
}

void SettlementLayoutManager::debugDraw() const {
    IHeightmapGrid& heightGrid = mWorld->getHeightmapGrid();
    TileCoord pos(mRootPos);
    const f32 WORLD_Z = 5.0f;
    f32v3 worldPos(pos.x, pos.y, WORLD_Z);
    const f32v3 WIDTH(3.0f, 3.0f, 0.0f);
    const ui32 FRAME_COUNT = 32;
    DebugRenderer::drawWireQuadThreadSafe(worldPos - WIDTH * 0.5f, WIDTH, color::Green, FRAME_COUNT);

    auto getWorldPosAtDTilePos = [&](DTileCoord pos) -> f32v3 {
        f32 z = heightGrid.getHeightAtVert<true>(pos);
        i32v2 tilePosA = pos.toTilePos();
        return f32v3(tilePosA.x, tilePosA.y, z);
    };
    for (auto& sector : mSectors) {
        f32v3 sectorWorldPos = getWorldPosAtDTilePos(sector.center);
        DebugRenderer::drawWireQuadThreadSafe(sectorWorldPos - WIDTH * 0.5f, WIDTH, color::Red, FRAME_COUNT);
    }
    for (auto& segment : mRoadNetwork.roadSegments) {
        color4 color = color::Yellow;
        f32v3 worldPosA = getWorldPosAtDTilePos(segment.segmentVerts[0]);
        f32v3 worldPosB = getWorldPosAtDTilePos(segment.segmentVerts.back());
        f32v3 infiniteRayOffset = glm::normalize(worldPosB - worldPosA);
        DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosA, worldPosB, color, FRAME_COUNT);
        if (segment.infiniteEdges[0]) {
            DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosA, worldPosA - infiniteRayOffset, color::Cyan, FRAME_COUNT);
        }
        if (segment.infiniteEdges[1]) {
            DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosB, worldPosB + infiniteRayOffset, color::Cyan, FRAME_COUNT);
        }
    }
}

bool SettlementRoadNetworkNew::simpleTraceAgainstSolidRoadSegments(DTileCoord start, DTileCoord end) {
    for (RoadSegmentID id = 0; id < roadSegments.size(); ++id) {
        RoadSegment& segment = roadSegments[id];
        IntersectionHit2D hit;
        i32v2 t1 = segment.segmentVerts[0].v;
        i32v2 t2 = segment.segmentVerts.back().v;
        switch (segment.segmentType) {
            case RoadSegmentType::Segment:
                hit = IntersectionUtil::segmentSegmentIntersect(start.v, end.v, t1, t2);
                if (hit.didHit()) {
                    return true;
                }
                break;
            case RoadSegmentType::PositiveRay:
                hit = IntersectionUtil::segmentRayIntersect(start.v, end.v, t1, t2 - t1);
                if (hit.didHit() && hit.timeTarget <= 1.0f) {
                    return true;
                }
                break;
            case RoadSegmentType::NegativeRay:
                hit = IntersectionUtil::segmentRayIntersect(start.v, end.v, t2, t1 - t2);
                if (hit.didHit() && hit.timeTarget <= 1.0f) {
                    return true;
                }
                break;
            case RoadSegmentType::InfiniteLine:
                hit = IntersectionUtil::segmentLineIntersect(start.v, end.v, t1, t2 - t1);
                if (hit.didHit() && hit.timeTarget >= 0.0f && hit.timeTarget <= 1.0f) {
                    return true;
                }
                break;
            default:
                assert(false);
                break;

        }
    }
    return false;
}

std::pair<RoadSegmentHitResult, RoadSegmentHitResult> SettlementRoadNetworkNew::getClosestHitsToAnySegmentInEachDirection(DTileCoord start, f32v2 dir) {
    f32v2 closestTimeEachDir(FLT_MAX, FLT_MAX);
    std::pair<RoadSegmentHitResult, RoadSegmentHitResult> closestHitEachDir;
    IntersectionHit2D hit;
    // Helper
    auto checkIsBestHit = [&](RoadSegmentID id) {
        if (hit.didHit()) {
            if (hit.timeSource < 0) {
                if (-hit.timeSource < closestTimeEachDir.x) {
                    closestTimeEachDir.x = -hit.timeSource;
                    closestHitEachDir.first = RoadSegmentHitResult{ .hitSegmentId = id, .timeSource = hit.timeSource, .timeTarget = hit.timeTarget };
                }
            }
            else {
                if (hit.timeSource < closestTimeEachDir.y) {
                    closestTimeEachDir.y = hit.timeSource;
                    closestHitEachDir.second = RoadSegmentHitResult{ .hitSegmentId = id, .timeSource = hit.timeSource, .timeTarget = hit.timeTarget };
                }
            }
        }
    };
    for (RoadSegmentID id = 0; id < roadSegments.size(); ++id) {
        RoadSegment& segment = roadSegments[id];
        i32v2 t1 = segment.segmentVerts[0].v;
        i32v2 t2 = segment.segmentVerts.back().v;

        switch (segment.segmentType) {
            case RoadSegmentType::Segment:
                hit = IntersectionUtil::lineSegmentIntersect(start.v, dir, t1, t2);
                checkIsBestHit(id);
                break;
            case RoadSegmentType::PositiveRay:
                hit = IntersectionUtil::lineRayIntersect(start.v, dir, t1, t2 - t1);
                checkIsBestHit(id);
                break;
            case RoadSegmentType::NegativeRay:
                hit = IntersectionUtil::lineRayIntersect(start.v, dir, t2, t1 - t2);
                checkIsBestHit(id);
                break;
            case RoadSegmentType::InfiniteLine:
                hit = IntersectionUtil::lineLineIntersect(start.v, dir, t1, t2 - t1);
                checkIsBestHit(id);
                break;
            default:
                assert(false);
                break;
        }
    }
    return closestHitEachDir;
}

bool SettlementRoadNetworkNew::tryAddRoadBetweenSectorPoints(World& world, entt::entity settlement, DTileCoord sector1Pos, DTileCoord sector2Pos, DTileCoord midPoint, RoadType roadType, ui8 width) {
    f32v2 offsetf(sector2Pos.v - sector1Pos.v);

    // Rotate offset so it is a a cell border between our two sectors
    offsetf = MathUtil::rotateVector2DRad(offsetf, M_PI_2F);

    auto[negHit, posHit] = getClosestHitsToAnySegmentInEachDirection(midPoint, offsetf);
    // First segment doesn't have to connect to anything
    if (roadSegments.size()) [[likely]] {
        if (negHit.hitSegmentId == INVALID_ROAD_SEGMENT_ID && posHit.hitSegmentId == INVALID_ROAD_SEGMENT_ID) {
            return false;
        }
    }

    RoadSegment newSegment;
    newSegment.widthTiles[0] = width;
    newSegment.widthTiles[1] = width;
    newSegment.infiniteEdges[0] = true;
    newSegment.infiniteEdges[1] = true;
    newSegment.segmentVerts.resize(2);
    newSegment.roadType = roadType;

    auto setVertexPositionBasedOnHit = [&](RoadSegmentHitResult hit, DTileCoord& vertToSnap, f32 dirMult) {
        if (hit.hitSegmentId != INVALID_ROAD_SEGMENT_ID) {
            RoadSegment& hitSegment = roadSegments[hit.hitSegmentId];
            if (hit.timeTarget <= 0.0f) {
                // Hit infinite negative edge, snap back
                vertToSnap = hitSegment.segmentVerts[0];
            }
            else if (hit.timeTarget >= 1.0f) {
                // Hit infinite positive edge, snap back
                vertToSnap = hitSegment.segmentVerts.back();
            }
            else {
                // Hit somewhere on the solid segment
                // TODO: Need to do trace against subsegment?
                f32v2 hitSegOffset(hitSegment.segmentVerts.back().v - hitSegment.segmentVerts[0].v);
                vertToSnap = DTileCoord(i32v2(glm::round(f32v2(hitSegment.segmentVerts[0].v) + hitSegOffset * hit.timeTarget)));
            }
        }
        else {
            vertToSnap = DTileCoord(i32v2(glm::round(f32v2(midPoint.v) + dirMult * offsetf * 0.5f)));
        }
    };

    setVertexPositionBasedOnHit(negHit, newSegment.segmentVerts[0], -1.0f);
    setVertexPositionBasedOnHit(posHit, newSegment.segmentVerts.back(), 1.0f);

    if (newSegment.segmentVerts[0].v == newSegment.segmentVerts.back().v) {
        LOG_CRITICAL("HI");
        return false;
    }

    auto worldBoundsCheck = [&](i32v2 pos) -> bool {
        if (pos.x <= 0 || pos.y <= 0 || pos.x >= world.getWidthDTiles() - 1 || pos.y >= world.getWidthDTiles() - 1) [[unlikely]] {
            return false;
        }
        return true;
    };
    if (!worldBoundsCheck(newSegment.segmentVerts[0].v) || !worldBoundsCheck(newSegment.segmentVerts[1].v)) [[unlikely]] {
        return false;
    }
   
    return tryPlaceRoadInternal(world, settlement, std::move(newSegment));
}

bool SettlementRoadNetworkNew::tryPlaceRoadInternal(World& world, entt::entity settlement, RoadSegment&& newSegment) {
    // Assume bounds have been checked

    //constexpr ui32 POINT_COUNT = 4;
    //DTileCoord roadPoints[POINT_COUNT];
    //roadPoints[0] = baseVertex.pos;
    //roadPoints[POINT_COUNT - 1] = targetPos;
    //for (int i = 1; i < POINT_COUNT - 1; ++i) {
    //    // generate intermediate points
    //    roadPoints[i] = DTileCoord(i32v2(glm::round(vmath::lerp(f32v2(baseVertex.pos.v), f32v2(targetPos.v), f32(i) / (POINT_COUNT - 1)))));
    //    roadPoints[i].x += randomGenerator.getRandomIntInRange(-2, 2);
    //    roadPoints[i].y += randomGenerator.getRandomIntInRange(-2, 2);
    //}

    std::vector<DTileCoord>& verts = newSegment.segmentVerts;
    const DTileCoord startVertex = verts[0];
    const DTileCoord endVertex = verts.back();

    auto getDistanceSqToRoad = [](DTileCoord pos, std::vector<DTileCoord>& verts) -> f32 {
        f32 closestSq = MathUtil::computePointToLineSegmentDistanceSQ(pos.v, verts[0].v, verts[1].v);
        for (int i = 1; i < verts.size() - 1; ++i) {
            f32 distSq = MathUtil::computePointToLineSegmentDistanceSQ(pos.v, verts[i].v, verts[i + 1].v);
            if (distSq < closestSq) {
                closestSq = distSq;
            }
        }
        return closestSq;
    };


    i32 aabbPadding = (i32)glm::max(newSegment.widthTiles[0], newSegment.widthTiles[1]);
    DTileCoord offset = endVertex - startVertex;
    i32AABB2 aabb;
    DTileCoord xSpan;
    DTileCoord ySpan;
    if (startVertex.x < endVertex.x) {
        xSpan.x = startVertex.x - aabbPadding;
        xSpan.y = endVertex.x + aabbPadding;
    }
    else {
        xSpan.x = endVertex.x - aabbPadding;
        xSpan.y = startVertex.x + aabbPadding;
    }
    if (startVertex.y < endVertex.y) {
        ySpan.x = startVertex.y - aabbPadding;
        ySpan.y = endVertex.y + aabbPadding;
    }
    else {
        ySpan.x = endVertex.y - aabbPadding;
        ySpan.y = startVertex.y + aabbPadding;
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

            if (heightGrid.getHeightAtVert<true>(pos) <= 0.0f) {
                const f32 distanceSq = getDistanceSqToRoad(pos, verts);
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

                    const f32 distanceSq = getDistanceSqToRoad(pos, verts);
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

    newSegment.roadPointsNeedingConstruct = std::move(roadVertsThisEdge);
    newSegment.roadPointsNeedingConstruct.shrink_to_fit();

    // Connect new road to other segments and block infinite edges
    // BaseVertex
    // EndVertex
    for (RoadSegmentID id = 0; id < roadSegments.size(); ++id) {
        RoadSegment& otherSegment = roadSegments[id];
        if (otherSegment.segmentVerts[0] == startVertex) {
            otherSegment.infiniteEdges[0] = false;
            updateRoadSegmentType(otherSegment);
        }
        else if (otherSegment.segmentVerts.back() == endVertex) {
            otherSegment.infiniteEdges[1] = false;
            updateRoadSegmentType(otherSegment);
        }
    }

    // ==================== BEGIN DEBUG ====================
    // TODO: REMOVE ***DEBUG BUILD ROADS***
    SimChunkTileGrid& tileGrid = world.getSimTileGrid();
    constexpr f32 BLEND_THICKNESS = 1.0f;
    f32 baseWidthf(newSegment.widthTiles[0]);
    f32 endWidthf(newSegment.widthTiles[1]);
    for (DTileCoord pos : newSegment.roadPointsNeedingConstruct) {
        auto [closestSq, closestT] = MathUtil::computePointToLineSegmentDistanceSQAndT(pos.v, verts[0].v, verts[1].v);
        for (int i = 1; i < verts.size() - 1; ++i) {
            auto [distanceSq, time] = MathUtil::computePointToLineSegmentDistanceSQAndT(pos.v, verts[i].v, verts[i + 1].v);
            if (distanceSq < closestSq) {
                closestSq = distanceSq;
                closestT = time;
            }
        }
        f32 desiredThickness = lerp(baseWidthf, endWidthf, closestT) * 0.5f;
        if (closestSq <= SQ(desiredThickness)) {
            const f32 distance = sqrtf(closestSq);
            const f32 strength = glm::min((desiredThickness - distance) / BLEND_THICKNESS, 1.0f);
            if (roadGrid.setRoadPointIfHigherIntensity(pos, RoadPoint{ .strength = ui8(strength * 255), .type = e_cast(newSegment.roadType) })) {
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
    std::vector<DTileCoord>().swap(newSegment.roadPointsNeedingConstruct);
    // ==================== END DEBUG ====================
    roadSegments.emplace_back(std::move(newSegment));
    return true;
}

void SettlementRoadNetworkNew::updateRoadSegmentType(RoadSegment& segment) {
    if (segment.infiniteEdges[0] == false) {
        if (segment.infiniteEdges[1] == false) {
            segment.segmentType = RoadSegmentType::Segment;
        }
        else {
            segment.segmentType = RoadSegmentType::PositiveRay;
        }
    } else if (segment.infiniteEdges[1] == false) {
        segment.segmentType = RoadSegmentType::NegativeRay;
    } else [[unlikely]] {
        segment.segmentType = RoadSegmentType::InfiniteLine;
    }
}
