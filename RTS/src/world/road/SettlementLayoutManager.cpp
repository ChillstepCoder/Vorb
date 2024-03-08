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
#include "debugging/VisualLogger.h"

f32v3 helperGetWorldPosFromDTileCoord(DTileCoord pos, World* world) {
    const f32 z = world->getHeightmapGrid().getHeightAtVert<true>(pos);
    const i32v2 tilePosA = pos.toTilePos();
    return f32v3(tilePosA.x, tilePosA.y, z);
};
f32v3 helperGetWorldPosFrom2DPos(f32v2 pos, World* world) {
    const f32 z = world->getHeightmapGrid().computeHeightAtPoint<true>(pos);
    return f32v3(pos.x, pos.y, z);
};

void helperAddVisLogLineBetweenCoords(VisualLog* log, DTileCoord a, DTileCoord b, World* world, color4 color) {
    if (log) {
        log->addLineBetweenPoints(helperGetWorldPosFromDTileCoord(a, world), helperGetWorldPosFromDTileCoord(b, world), color);
    }
}

void helperAddVisLogFilledQuadAtCoord(VisualLog* log, DTileCoord p, f32v2 size, World* world, color4 color) {
    if (log) {
        const f32v3 pos = helperGetWorldPosFromDTileCoord(p, world);
        const f32v3 s3d(size.x, size.y, 0.0f);
        log->addFilledQuad(pos - s3d * 0.5f, size, color);
    }
}

void helperAddVisLogLineBetweenPos(VisualLog* log, f32v2 a, f32v2 b, World* world, color4 color) {
    if (log) {
        log->addLineBetweenPoints(helperGetWorldPosFrom2DPos(a, world), helperGetWorldPosFrom2DPos(b, world), color);
    }
}

void helperAddVisLogFilledQuadAtPos(VisualLog* log, f32v2 p, f32v2 size, World* world, color4 color) {
    if (log) {
        const f32v3 pos = helperGetWorldPosFrom2DPos(p, world);
        const f32v3 s3d(size.x, size.y, 0.0f);
        log->addFilledQuad(pos - s3d * 0.5f, size, color);
    }
}

void helperAddTextAtPos(VisualLog* log, f32v2 p, const std::string& text, World* world, color4 color) {
    if (log) {
        const f32v3 pos = helperGetWorldPosFrom2DPos(p, world);
        log->addText(text, pos, 1.0f, f32v2(0.0f, 1.0f), color);
    }
}

bool SettlementLayoutManager::tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos) {

    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("Settlement: " + std::to_string(dTilePos.v.x) + "," + std::to_string(dTilePos.v.y), VisualLogCategory::Settlement, true);
    if (visLog) {
        const TileCoord tPos(dTilePos);
        visLog->setCameraDistanceCheckPosOffset(f32v3(tPos.x, tPos.y, 0.0f));
        mCurrentVisLog = visLog;
        mRoadNetwork.mCurrentVisLog = visLog;
    }

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
    addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)PADDED_RADIUS * 3.3f), DESIRED_RADIUS);
    addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS * 2.0f, (i32)PADDED_RADIUS * 4.3f), DESIRED_RADIUS);
   
    if (mCurrentVisLog) {
        visLog->finish();
        mCurrentVisLog = nullptr;
        mRoadNetwork.mCurrentVisLog = nullptr;
    }
    return addedCount > 1;
}

bool SettlementLayoutManager::tryAddSector(entt::entity settlement, DTileCoord center, f32 desiredRadius) {

    if (mSectors.empty()) [[unlikely]] {
        mSectors.emplace_back(center, desiredRadius, mSectors.size());
        return true;
    }

    if (mCurrentVisLog) {
        mCurrentVisLog->nextStep("Sector Attempt - " + std::to_string(mSectors.size()));
        helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, center, f32v2(3.0f), mWorld, color::Green);
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
        if (!mRoadNetwork.simpleTraceAgainstSolidRoadSegments(newSector.center.v, otherSector.center.v)) {
            helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSector.center, otherSector.center, mWorld, color::Green);
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
        else {
            helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSector.center, otherSector.center, mWorld, color::Red);
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
    // TODO: RE-ENABLE WITH TOGGLE
    return;

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
    ui32 i = 0;
    for (auto& segment : mRoadNetwork.roadSegments) {
        color4 color = color::Yellow;
        f32v3 worldPosA = getWorldPosAtDTilePos(segment.segmentVerts[0]) + f32v3(0.0f, 0.0f, i);
        f32v3 worldPosB = getWorldPosAtDTilePos(segment.segmentVerts.back()) + f32v3(0.0f, 0.0f, i);
        f32v3 infiniteRayOffset(segment.direction.x, segment.direction.y, 0.0f);
        DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosA, worldPosB, color, FRAME_COUNT);
        if (segment.infiniteEdges[0]) {
            DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosA, worldPosA - infiniteRayOffset, color::Cyan, FRAME_COUNT);
        }
        if (segment.infiniteEdges[1]) {
            DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosB, worldPosB + infiniteRayOffset, color::Cyan, FRAME_COUNT);
        }
        ++i;
    }
}

bool SettlementRoadNetworkNew::simpleTraceAgainstSolidRoadSegments(f32v2 start, f32v2 end) {
    for (RoadSegmentID id = 0; id < roadSegments.size(); ++id) {
        RoadSegment& segment = roadSegments[id];
        IntersectionHit2D hit;
        i32v2 t1 = segment.segmentVerts[0].v;
        i32v2 t2 = segment.segmentVerts.back().v;
        switch (segment.segmentType) {
            case RoadSegmentType::Segment:
                hit = IntersectionUtil::segmentSegmentIntersect(start, end, t1, t2);
                if (hit.didHit()) {
                    return true;
                }
                break;
            case RoadSegmentType::PositiveRay:
                hit = IntersectionUtil::segmentRayIntersect(start, end, t1, t2 - t1);
                if (hit.didHit() && hit.timeTarget <= 1.0f) {
                    return true;
                }
                break;
            case RoadSegmentType::NegativeRay:
                hit = IntersectionUtil::segmentRayIntersect(start, end, t2, t1 - t2);
                if (hit.didHit() && hit.timeTarget <= 1.0f) {
                    return true;
                }
                break;
            case RoadSegmentType::InfiniteLine:
                hit = IntersectionUtil::segmentLineIntersect(start, end, t1, t2 - t1);
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

bool SettlementRoadNetworkNew::simpleTraceAgainstSolidRoadSegmentsWithExclusions(DTileCoord start, DTileCoord end, std::span<RoadSegmentID> exclusions) {
    for (RoadSegmentID id = 0; id < roadSegments.size(); ++id) {
        bool exclude = false;
        for (RoadSegmentID exclusion : exclusions) {
            if (exclusion == id) [[unlikely]] {
                exclude = true;
                break;
            }
        }
        if (exclude) [[unlikely]] {
            continue;
        }
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

    helperAddVisLogLineBetweenCoords(mCurrentVisLog, DTileCoord(f32v2(midPoint.v) - offsetf * 0.5f), DTileCoord(f32v2(midPoint.v) + offsetf * 0.5f), &world, color4(1.0f, 1.0f, 1.0f, 0.5f));

    auto[negHit, posHit] = getClosestHitsToAnySegmentInEachDirection(midPoint, offsetf);
    constexpr f32 MAX_TIME = 400.0f;
    // Make sure our hit didn't happen too far away
    // TODO: Also time target?
    if (abs(negHit.timeSource) > MAX_TIME)  {
        negHit.hitSegmentId = INVALID_ROAD_SEGMENT_ID;
    }
    if (abs(posHit.timeSource) > MAX_TIME) {
        posHit.hitSegmentId = INVALID_ROAD_SEGMENT_ID;
    }
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

    RoadSegmentID exclusionList[2];
    ui32 exclusionSize = 0;
    auto setVertexPositionBasedOnHit = [&](RoadSegmentHitResult hit, DTileCoord& vertToSnap, f32 dirMult) {
        if (hit.hitSegmentId != INVALID_ROAD_SEGMENT_ID) {
            RoadSegment& hitSegment = roadSegments[hit.hitSegmentId];
            helperAddVisLogFilledQuadAtPos(mCurrentVisLog, f32v2(TileCoord(hitSegment.segmentVerts[0]).v) + hitSegment.direction * hit.timeTarget * hitSegment.length * 2.0f, f32v2(2.0f), &world, color::Yellow);
            helperAddTextAtPos(mCurrentVisLog, f32v2(TileCoord(hitSegment.segmentVerts[0]).v) + hitSegment.direction * hit.timeTarget * hitSegment.length * 2.0f, std::to_string(hit.timeTarget), &world, color::Yellow);
            helperAddVisLogLineBetweenCoords(mCurrentVisLog, hitSegment.segmentVerts[0], hitSegment.segmentVerts.back(), &world, color::Yellow);
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
            exclusionList[exclusionSize++] = hit.hitSegmentId;
        }
        else {
            vertToSnap = DTileCoord(i32v2(glm::round(f32v2(midPoint.v) + dirMult * offsetf * 0.5f)));
        }
    };
    setVertexPositionBasedOnHit(negHit, newSegment.segmentVerts[0], -1.0f);
    setVertexPositionBasedOnHit(posHit, newSegment.segmentVerts.back(), 1.0f);

    if (newSegment.segmentVerts[0].v == newSegment.segmentVerts.back().v) {
        helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(3.0f), &world, color::Red);
        // LOG_CRITICAL("ZERO LENGTH ROAD SEGMENT DETECTED");
        // return false;
        newSegment.segmentVerts[0] = DTileCoord(f32v2(midPoint.v) - offsetf * 0.5f);
        newSegment.segmentVerts.back() = DTileCoord(f32v2(midPoint.v) + offsetf * 0.5f);
    }
    helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(2.0f), &world, color::LightGreen);
    helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts.back(), f32v2(2.0f), &world, color::LightGreen);

    f32v2 offset = f32v2(newSegment.segmentVerts.back().v - newSegment.segmentVerts[0].v);
    newSegment.length = glm::length(offset);
    newSegment.direction = offset / newSegment.length;

    // Check collision against any roads that aren't our target connecting roads
    if (simpleTraceAgainstSolidRoadSegments(
        f32v2(newSegment.segmentVerts[0].v) + newSegment.direction * 0.5f, f32v2(newSegment.segmentVerts.back().v) - newSegment.direction * 0.5f/*, std::span<RoadSegmentID>(exclusionList, exclusionSize)*/)) {
        helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSegment.segmentVerts[0], newSegment.segmentVerts.back(), &world, color::Black);
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
   
    DTileCoord start = newSegment.segmentVerts[0];
    DTileCoord end = newSegment.segmentVerts.back();
    const bool place = tryPlaceRoadInternal(world, settlement, std::move(newSegment));
    helperAddVisLogLineBetweenCoords(mCurrentVisLog, start, end, &world, place ? color::LightGreen : color::Gray);
    return place;
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
    constexpr f32 DOT_THRESHOLD = 0.93969262078; // cos(20) degrees is the threshold for blocking an infinite edge
    for (RoadSegmentID id = 0; id < roadSegments.size(); ++id) {
        RoadSegment& otherSegment = roadSegments[id];
        if (startVertex == otherSegment.segmentVerts[0]) {
            // Base vertices are overlapping, so if they are pointing in opposite dir, block the infinite edges
            if (glm::dot(newSegment.direction, otherSegment.direction) < -DOT_THRESHOLD) {
                otherSegment.infiniteEdges[0] = false;
                newSegment.infiniteEdges[0] = false;
                updateRoadSegmentType(otherSegment);
            }
        }
        else if (endVertex == otherSegment.segmentVerts[0]) {
            // End vertex overlaps base, so if they are in same dir, block the infinite edges
            if (glm::dot(newSegment.direction, otherSegment.direction) > DOT_THRESHOLD) {
                otherSegment.infiniteEdges[0] = false;
                newSegment.infiniteEdges[1] = false;
                updateRoadSegmentType(otherSegment);
            }
        }
        if (startVertex == otherSegment.segmentVerts.back()) {
            // Base vertex overlaps other end, so if they are in same dir, block the infinite edges
            if (glm::dot(newSegment.direction, otherSegment.direction) > DOT_THRESHOLD) {
                otherSegment.infiniteEdges[1] = false;
                newSegment.infiniteEdges[0] = false;
                updateRoadSegmentType(otherSegment);
            }
        }
        else if (endVertex == otherSegment.segmentVerts.back()) {
            // End vertices overlap, so if they are pointing away from each other, block the infinite edges
            if (glm::dot(newSegment.direction, otherSegment.direction) < -DOT_THRESHOLD) {
                otherSegment.infiniteEdges[1] = false;
                newSegment.infiniteEdges[1] = false;
                updateRoadSegmentType(otherSegment);
            }
        }
    }

    updateRoadSegmentType(newSegment);

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
