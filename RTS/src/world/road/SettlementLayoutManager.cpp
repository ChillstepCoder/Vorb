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

// Helper
bool isInfiniteTime(f32 time) {
    return time < 0.0f || time > 1.0f;
}

f32v3 helperGetWorldPosFromDTileCoord(DTileCoord pos, World* world) {
    if (pos.x < 0 || pos.y < 0 || pos.x > world->getWidthDTiles() || pos.y > world->getWidthDTiles()) [[unlikely]] {
        return f32v3(0.0f);
    }
    const f32 z = world->getHeightmapGrid().getHeightAtVert<true>(pos);
    const i32v2 tilePosA = pos.toTilePos();
    return f32v3(tilePosA.x, tilePosA.y, z);
};
f32v3 helperGetWorldPosFrom2DPos(f32v2 pos, World* world) {
    if (pos.x < 0 || pos.y < 0 || pos.x > world->getWidthTiles() || pos.y > world->getWidthTiles()) [[unlikely]] {
        return f32v3(0.0f);
    }
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
        log->addText(text, pos, 2.0f, f32v2(0.0f, 2.5f), color);
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
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS, (i32)PADDED_RADIUS), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)-PADDED_RADIUS), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)PADDED_RADIUS), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)PADDED_RADIUS * 3.3f), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS * 2.0f, (i32)PADDED_RADIUS * 4.3f), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS * 3.0f, (i32)PADDED_RADIUS), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS * 3.0f, (i32)-PADDED_RADIUS), DESIRED_RADIUS);
   
    for (ui32 i = 0; i < 32; ++i) {
        addedCount += tryAddNewRandomSector(settlement);
    }

    if (mCurrentVisLog) {
        visLog->finish();
        mCurrentVisLog = nullptr;
        mRoadNetwork.mCurrentVisLog = nullptr;
    }
    return addedCount > 1;
}

bool SettlementLayoutManager::tryAddNewRandomSector(entt::entity settlement) {
    const f32 desiredRadius = 16.f + mRoadNetwork.randomGenerator.getRandomFloatUnsigned() * 8.0f;
    constexpr ui32 TRY_COUNT = 16;
    for (ui32 i = 0; i < TRY_COUNT; ++i) {
        const ui32 sectorIndex = mRoadNetwork.randomGenerator.getRandomUIntInRange(0, mSectors.size());
        const SettlementSector& sector = mSectors[sectorIndex];
        const f32 distance = sector.desiredRadius + desiredRadius + 2.0f;
        const f32 angle = mRoadNetwork.randomGenerator.getRandomFloatUnsigned() * glm::two_pi<f32>();
        // TODO: GetNormalDir util
        const f32v2 offset = MathUtil::rotateVector2DRad(f32v2(distance, 0.0f), angle);
        const DTileCoord newPos = sector.center + DTileCoord(i32v2(glm::round(offset)));
        if (tryAddSector(settlement, newPos, desiredRadius)) {
            return true;
        }
    }
    return false;
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

    if (mCurrentVisLog) {
        mCurrentVisLog->nextStep("Sector Attempt - " + std::to_string(mSectors.size()));
        helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, center, f32v2(3.0f), mWorld, color::Green);
    }

    SettlementSector newSector(center, desiredRadius, mSectors.size());

    constexpr ui32 DESIRED_ROAD_WIDTH = 5;

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

RoadSegmentIntersectBestHits SettlementRoadNetworkNew::getBestRoadSegmentHitsForNewPlacement(DTileCoord start, f32v2 dir) {
    f32v2 closestSegmentTimeEachDir(FLT_MAX, FLT_MAX);
    f32v2 closestInfiniteTimeEachDir(FLT_MAX, FLT_MAX);
    RoadSegmentIntersectBestHits bestHits;
    IntersectionHit2D hit;
    // Helper
    auto checkIsBestHit = [&](RoadSegmentID id) {
        if (hit.didHit()) {
            if (isInfiniteTime(hit.timeTarget)) {
                if (hit.timeSource < 0) {
                    if (-hit.timeSource < closestInfiniteTimeEachDir.x) {
                        closestInfiniteTimeEachDir.x = -hit.timeSource;
                        bestHits.negativeInfiniteHit = RoadSegmentHitResult{ .hitSegmentId = id, .timeSource = hit.timeSource, .timeTarget = hit.timeTarget };
                    }
                }
                else {
                    if (hit.timeSource < closestInfiniteTimeEachDir.y) {
                        closestInfiniteTimeEachDir.y = hit.timeSource;
                        bestHits.positiveInfiniteHit = RoadSegmentHitResult{ .hitSegmentId = id, .timeSource = hit.timeSource, .timeTarget = hit.timeTarget };
                    }
                }
            }
            else {
                if (hit.timeSource < 0) {
                    if (-hit.timeSource < closestSegmentTimeEachDir.x) {
                        closestSegmentTimeEachDir.x = -hit.timeSource;
                        bestHits.negativeSegmentHit = RoadSegmentHitResult{ .hitSegmentId = id, .timeSource = hit.timeSource, .timeTarget = hit.timeTarget };
                    }
                }
                else {
                    if (hit.timeSource < closestSegmentTimeEachDir.y) {
                        closestSegmentTimeEachDir.y = hit.timeSource;
                        bestHits.positiveSegmentHit = RoadSegmentHitResult{ .hitSegmentId = id, .timeSource = hit.timeSource, .timeTarget = hit.timeTarget };
                    }
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
    return bestHits;
}

bool SettlementRoadNetworkNew::tryAddRoadBetweenSectorPoints(World& world, entt::entity settlement, DTileCoord sector1Pos, DTileCoord sector2Pos, DTileCoord midPoint, RoadType roadType, ui8 width) {
    f32v2 offsetf(sector2Pos.v - sector1Pos.v);

    // Rotate offset so it is a a cell border between our two sectors
    offsetf = MathUtil::rotateVector2DRad(offsetf, M_PI_2F);
    const DTileCoord startRoadPos1 = DTileCoord(f32v2(midPoint.v) - offsetf * 0.5f);
    const DTileCoord startRoadPos2 = DTileCoord(f32v2(midPoint.v) + offsetf * 0.5f);

    helperAddVisLogLineBetweenCoords(mCurrentVisLog, startRoadPos1, startRoadPos2, &world, color4(1.0f, 1.0f, 1.0f, 0.5f));

    RoadSegmentIntersectBestHits bestHits = getBestRoadSegmentHitsForNewPlacement(midPoint, offsetf);
    // TODO: Fallback to second best
    RoadSegmentHitResult bestNegative = bestHits.negativeSegmentHit.isValid() ? bestHits.negativeSegmentHit : bestHits.negativeInfiniteHit;
    RoadSegmentHitResult bestPositive = bestHits.positiveSegmentHit.isValid() ? bestHits.positiveSegmentHit : bestHits.positiveInfiniteHit;

    constexpr f32 MAX_TIME = 400.0f;
    // Make sure our hit didn't happen too far away
    // TODO: Also time target?
    if (abs(bestNegative.timeSource) > MAX_TIME)  {
        bestNegative.hitSegmentId = INVALID_ROAD_SEGMENT_ID;
    }
    if (abs(bestPositive.timeSource) > MAX_TIME) {
        bestPositive.hitSegmentId = INVALID_ROAD_SEGMENT_ID;
    }
    // First segment doesn't have to connect to anything
    if (roadSegments.size()) [[likely]] {
        if (!bestNegative.isValid() && !bestPositive.isValid()) {
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

    // List of roads we need to check that we aren't overlapping
    RoadSegment* checkOverlapList[2];
    ui32 overlapCheckCount = 0;
    // If we snap twice, it means we have an additional fallback behavior to try on failure
    f32v2 infiniteHitTilePositions[2];
    ui32 infiniteEdgeSnapCount = 0;
    auto setVertexPositionBasedOnHit = [&](RoadSegmentHitResult hit, DTileCoord& vertToSnap, f32 dirMult) {
        if (hit.hitSegmentId != INVALID_ROAD_SEGMENT_ID) {
            RoadSegment& hitSegment = roadSegments[hit.hitSegmentId];
            const f32v2 hitPoint = f32v2(TileCoord(hitSegment.segmentVerts[0]).v) + hitSegment.direction * hit.timeTarget * hitSegment.length * 2.0f;
            helperAddVisLogFilledQuadAtPos(mCurrentVisLog, hitPoint, f32v2(2.0f), &world, color::Yellow);
            helperAddTextAtPos(mCurrentVisLog, hitPoint, std::to_string(hit.timeTarget), &world, color::Yellow);
            helperAddVisLogLineBetweenCoords(mCurrentVisLog, hitSegment.segmentVerts[0], hitSegment.segmentVerts.back(), &world, color::Yellow);
            if (hit.timeTarget <= 0.0f) {
                // Hit infinite negative edge, snap back
                infiniteHitTilePositions[infiniteEdgeSnapCount++] = hitPoint;
                vertToSnap = hitSegment.segmentVerts[0];
                helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, vertToSnap, f32v2(2.0f), &world, color::Blue);
            }
            else if (hit.timeTarget >= 1.0f) {
                // Hit infinite positive edge, snap back
                infiniteHitTilePositions[infiniteEdgeSnapCount++] = hitPoint;
                vertToSnap = hitSegment.segmentVerts.back();
                helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, vertToSnap, f32v2(2.0f), &world, color::Blue);
            }
            else {
                // Hit somewhere on the solid segment
                // TODO: Need to do trace against subsegment?
                f32v2 hitSegOffset(hitSegment.segmentVerts.back().v - hitSegment.segmentVerts[0].v);
                vertToSnap = DTileCoord(i32v2(glm::round(f32v2(hitSegment.segmentVerts[0].v) + hitSegOffset * hit.timeTarget)));
                helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, vertToSnap, f32v2(2.0f), &world, color::LightBlue);
            
                // This can result in us completely overlapping the segment, so we need to check for that
                checkOverlapList[overlapCheckCount++] = &hitSegment;
            }
        }
        else {
            vertToSnap = DTileCoord(i32v2(glm::round(f32v2(midPoint.v) + dirMult * offsetf * 0.5f)));
        }
    };
    setVertexPositionBasedOnHit(bestNegative, newSegment.segmentVerts[0], -1.0f);
    setVertexPositionBasedOnHit(bestPositive, newSegment.segmentVerts.back(), 1.0f);

    // Helpers
    auto worldBoundsCheck = [&](i32v2 pos) -> bool {
        if (pos.x <= 0 || pos.y <= 0 || pos.x >= world.getWidthDTiles() - 1 || pos.y >= world.getWidthDTiles() - 1) [[unlikely]] {
            return false;
        }
        return true;
    };
    auto segmentIsValid = [this, sector1Pos, sector2Pos, &worldBoundsCheck](f32v2 v1, f32v2 v2, f32v2 dir) -> bool {
        // Check if our segment is between the two sectors
        IntersectionHit2D hit = IntersectionUtil::segmentSegmentIntersect(v1, v2, sector1Pos.v, sector2Pos.v);
        if (!hit.didHit()) {
            return false;
        }
        // Check collision against any roads that aren't our target connecting roads
        if (simpleTraceAgainstSolidRoadSegments(v1 + dir * 0.5f, v2 - dir * 0.5f)) {
            return false;
        }
        if (!worldBoundsCheck(v1) || !worldBoundsCheck(v2)) [[unlikely]] {
            return false;
        }
        return true;
    };
    
    bool needTryFallback = false;

    if (newSegment.segmentVerts[0].v == newSegment.segmentVerts.back().v) {
        // Both verts collapsed to a single point, a common failure case.
        // We can do a fallback by not collapsing each of the verts one at a time and checking validity

        // Rare failure case, just fail
        if (overlapCheckCount != 0) {
            return false;
        }
        // If we didn't snap on both ends, we can't try the alternate fallback
        if (infiniteEdgeSnapCount < 2) {
            helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(3.0f), &world, color::Red);
            return false;
        }
        helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(3.0f), &world, color::Orange);
        
        needTryFallback = true;
    }
    else {
        // Standard case, check validity and overlaps
        const f32v2 offset = f32v2(newSegment.segmentVerts.back().v - newSegment.segmentVerts[0].v);
        newSegment.length = glm::length(offset);
        newSegment.direction = offset / newSegment.length;

        // Its possible we collapse on top of another segment, this checks that
        constexpr f32 OVERLAP_DOT_THRESHOLD = 0.99619469809; // About cos(5deg)
        for (ui32 c = 0; c < overlapCheckCount; ++c) {
            RoadSegment& segment = *checkOverlapList[c];
            if (abs(glm::dot(newSegment.direction, segment.direction)) > OVERLAP_DOT_THRESHOLD) {
                helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSegment.segmentVerts[0], newSegment.segmentVerts.back(), &world, color::DarkRed);
                needTryFallback = true;
                break;
            }
        }

        if (!needTryFallback && !segmentIsValid(newSegment.segmentVerts[0].v, newSegment.segmentVerts.back().v, newSegment.direction)) {
            helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSegment.segmentVerts[0], newSegment.segmentVerts.back(), &world, color::Black);
            needTryFallback = true;
        }
    }
   
    if (needTryFallback) {
        // Alternate fallback
        // Try first fallback
        f32v2 v1 = startRoadPos1.v/*infiniteHitTilePositions[0] * 0.5f*/;
        f32v2 v2 = newSegment.segmentVerts.back().v;

        if (segmentIsValid(v1, v2, glm::normalize(v2 - v1))) {
            // Round to nearest tilecoord
            newSegment.segmentVerts[0] = DTileCoord(i32v2(glm::round(v1)));
            helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(2.0f), &world, color::LightPink);
        }
        else {
            // Try second fallback
            v1 = newSegment.segmentVerts[0].v;
            v2 = startRoadPos2.v/*infiniteHitTilePositions[1] * 0.5f*/;
            if (segmentIsValid(v1, v2, glm::normalize(v2 - v1))) {
                newSegment.segmentVerts.back() = DTileCoord(i32v2(glm::round(v2)));
                helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts.back(), f32v2(2.0f), &world, color::LightPink);
            }
            else {
                helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSegment.segmentVerts[0], newSegment.segmentVerts.back(), &world, color::DarkGray);
                return false;
            }
        }
        const f32v2 offset = f32v2(newSegment.segmentVerts.back().v - newSegment.segmentVerts[0].v);
        newSegment.length = glm::length(offset);
        newSegment.direction = offset / newSegment.length;
    }

    helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(2.0f), &world, color::LightGreen);
    helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts.back(), f32v2(2.0f), &world, color::LightGreen);

    DTileCoord start = newSegment.segmentVerts[0];
    DTileCoord end = newSegment.segmentVerts.back();
    const bool place = tryPlaceRoadInternal(world, settlement, std::move(newSegment));
    helperAddVisLogLineBetweenCoords(mCurrentVisLog, start, end, &world, place ? color::LightGreen : color::Gray);
    return place;
}

bool SettlementRoadNetworkNew::tryPlaceRoadInternal(World& world, entt::entity settlement, RoadSegment&& newSegment) {
    // Assume bounds have been checked
    constexpr ui32 POINT_COUNT = 4;

    // Expand to the point count
    std::vector<DTileCoord>& verts = newSegment.segmentVerts;
    assert(verts.size() == 2);
    verts.resize(POINT_COUNT);
    verts.back() = verts[1]; // 1 was previous last
    const DTileCoord startVertex = verts[0];
    const DTileCoord endVertex = verts.back();

    for (int i = 1; i < POINT_COUNT - 1; ++i) {
        // generate intermediate points
        verts[i] = DTileCoord(i32v2(glm::round(vmath::lerp(f32v2(startVertex.v), f32v2(endVertex.v), f32(i) / (POINT_COUNT - 1)))));
        verts[i].x += randomGenerator.getRandomIntInRange(-2, 2);
        verts[i].y += randomGenerator.getRandomIntInRange(-2, 2);
    }

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
