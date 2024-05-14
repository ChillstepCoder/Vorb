#include "stdafx.h"
#include "SettlementRoadNetwork.h"

#include "math/Random.h"
#include "world/World.h"
#include "world/ownership/OwnershipGrid.h"
#include "world/road/TerrainSurfaceGrid.h"
#include "world/IHeightmapGrid.h"
#include "world/chunk/SimChunkTileGrid.h"

#include "world/settlement/SettlementPlotManager.h"

#include "util/IntersectionUtil.h"
#include "util/MathUtil.hpp"

#include "debugging/DebugRenderer.h"
#include "debugging/VisualLogger.h"

#include "world/settlement/SettlementDebugHelpers.inl"

i32AABB2 getAABBFromRoadSegment(ui8 startWidth, ui8 endWidth, DTileCoord startPos, DTileCoord endPos) {
    i32 aabbPadding = (i32)glm::max(startWidth, endWidth);
    DTileCoord offset = endPos - startPos;
    i32AABB2 aabb;
    DTileCoord xSpan;
    DTileCoord ySpan;
    if (startPos.x < endPos.x) {
        xSpan.x = startPos.x - aabbPadding;
        xSpan.y = endPos.x + aabbPadding;
    }
    else {
        xSpan.x = endPos.x - aabbPadding;
        xSpan.y = startPos.x + aabbPadding;
    }
    if (startPos.y < endPos.y) {
        ySpan.x = startPos.y - aabbPadding;
        ySpan.y = endPos.y + aabbPadding;
    }
    else {
        ySpan.x = endPos.y - aabbPadding;
        ySpan.y = startPos.y + aabbPadding;
    }

    aabb.pos = i32v2(xSpan.x, ySpan.x);
    aabb.dims = i32v2(xSpan.y - xSpan.x, ySpan.y - ySpan.x);
    return aabb;
}

SettlementRoadNetwork::SettlementRoadNetwork(World& world, RandomGenerator& randomGenerator) : mWorld(world), mRandomGenerator(randomGenerator) {

}

SettlementRoadNetwork::~SettlementRoadNetwork() = default;

void SettlementRoadNetwork::init(SettlementPlotManager& plotManager) {
    mPlotManager = &plotManager;
}


bool SettlementRoadNetwork::tryAddRoadBetweenSectorPoints(entt::entity settlement, DTileCoord sector1Pos, DTileCoord sector2Pos, DTileCoord midPoint, TerrainSurfaceType roadType, ui8 width, SettlementZone zone) {
    f32v2 offsetf(sector2Pos.v - sector1Pos.v);

    // Rotate offset so it is a a cell border between our two sectors
    offsetf = MathUtil::rotateVector2DRad(offsetf, M_PI_2F);
    const DTileCoord startRoadPos1 = DTileCoord(f32v2(midPoint.v) - offsetf * 0.5f);
    const DTileCoord startRoadPos2 = DTileCoord(f32v2(midPoint.v) + offsetf * 0.5f);

    helperAddVisLogLineBetweenCoords(mCurrentVisLog, startRoadPos1, startRoadPos2, &mWorld, color4(1.0f, 1.0f, 1.0f, 0.5f));

    RoadSegmentIntersectBestHits bestHits = getBestRoadSegmentHitsForNewPlacement(midPoint, offsetf);
    // TODO: Fallback to second best
    RoadSegmentHitResult bestNegative = bestHits.negativeSegmentHit.isValid() ? bestHits.negativeSegmentHit : bestHits.negativeInfiniteHit;
    RoadSegmentHitResult bestPositive = bestHits.positiveSegmentHit.isValid() ? bestHits.positiveSegmentHit : bestHits.positiveInfiniteHit;

    constexpr f32 MAX_TIME = 400.0f;
    // Make sure our hit didn't happen too far away
    // TODO: Also time target?
    if (abs(bestNegative.timeSource) > MAX_TIME) {
        bestNegative.hitSegmentId = INVALID_ROAD_SEGMENT_ID;
    }
    if (abs(bestPositive.timeSource) > MAX_TIME) {
        bestPositive.hitSegmentId = INVALID_ROAD_SEGMENT_ID;
    }
    // First segment doesn't have to connect to anything
    if (mRoadSegments.size()) [[likely]] {
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
    newSegment.zone = zone;

    // List of roads we need to check that we aren't overlapping
    RoadSegment* checkOverlapList[2];
    ui32 overlapCheckCount = 0;
    // If we snap twice, it means we have an additional fallback behavior to try on failure
    f32v2 infiniteHitTilePositions[2];
    ui32 infiniteEdgeSnapCount = 0;
    auto setVertexPositionBasedOnHit = [&](RoadSegmentHitResult hit, DTileCoord& vertToSnap, f32 dirMult) {
        if (hit.hitSegmentId != INVALID_ROAD_SEGMENT_ID) {
            RoadSegment& hitSegment = mRoadSegments[hit.hitSegmentId];
            const f32v2 hitPoint = f32v2(TileCoord(hitSegment.segmentVerts[0]).v) + hitSegment.direction * hit.timeTarget * hitSegment.length * 2.0f;
            helperAddVisLogFilledQuadAtPos(mCurrentVisLog, hitPoint, f32v2(2.0f), &mWorld, color::Yellow);
            helperAddTextAtPos(mCurrentVisLog, hitPoint, std::to_string(hit.timeTarget), &mWorld, color::Yellow);
            helperAddVisLogLineBetweenCoords(mCurrentVisLog, hitSegment.segmentVerts[0], hitSegment.segmentVerts.back(), &mWorld, color::Yellow);
            if (hit.timeTarget <= 0.0f) {
                // Hit infinite negative edge, snap back
                infiniteHitTilePositions[infiniteEdgeSnapCount++] = hitPoint;
                vertToSnap = hitSegment.segmentVerts[0];
                helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, vertToSnap, f32v2(2.0f), &mWorld, color::Blue);
            }
            else if (hit.timeTarget >= 1.0f) {
                // Hit infinite positive edge, snap back
                infiniteHitTilePositions[infiniteEdgeSnapCount++] = hitPoint;
                vertToSnap = hitSegment.segmentVerts.back();
                helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, vertToSnap, f32v2(2.0f), &mWorld, color::Blue);
            }
            else {
                // Hit somewhere on the solid segment
                // TODO: Need to do trace against subsegment?
                f32v2 hitSegOffset(hitSegment.segmentVerts.back().v - hitSegment.segmentVerts[0].v);
                vertToSnap = DTileCoord(i32v2(glm::round(f32v2(hitSegment.segmentVerts[0].v) + hitSegOffset * hit.timeTarget)));
                helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, vertToSnap, f32v2(2.0f), &mWorld, color::LightBlue);

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
        if (pos.x <= 0 || pos.y <= 0 || pos.x >= mWorld.getWidthDTiles() - 1 || pos.y >= mWorld.getWidthDTiles() - 1) [[unlikely]] {
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
            helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(3.0f), &mWorld, color::Red);
            return false;
        }
        helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(3.0f), &mWorld, color::Orange);

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
                helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSegment.segmentVerts[0], newSegment.segmentVerts.back(), &mWorld, color::DarkRed);
                needTryFallback = true;
                break;
            }
        }

        if (!needTryFallback && !segmentIsValid(newSegment.segmentVerts[0].v, newSegment.segmentVerts.back().v, newSegment.direction)) {
            helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSegment.segmentVerts[0], newSegment.segmentVerts.back(), &mWorld, color::Black);
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
            helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(2.0f), &mWorld, color::LightPink);
        }
        else {
            // Try second fallback
            v1 = newSegment.segmentVerts[0].v;
            v2 = startRoadPos2.v/*infiniteHitTilePositions[1] * 0.5f*/;
            if (segmentIsValid(v1, v2, glm::normalize(v2 - v1))) {
                newSegment.segmentVerts.back() = DTileCoord(i32v2(glm::round(v2)));
                helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts.back(), f32v2(2.0f), &mWorld, color::LightPink);
            }
            else {
                helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSegment.segmentVerts[0], newSegment.segmentVerts.back(), &mWorld, color::DarkGray);
                return false;
            }
        }
        const f32v2 offset = f32v2(newSegment.segmentVerts.back().v - newSegment.segmentVerts[0].v);
        newSegment.length = glm::length(offset);
        newSegment.direction = offset / newSegment.length;
    }

    helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts[0], f32v2(2.0f), &mWorld, color::LightGreen);
    helperAddVisLogFilledQuadAtCoord(mCurrentVisLog, newSegment.segmentVerts.back(), f32v2(2.0f), &mWorld, color::LightGreen);

    DTileCoord start = newSegment.segmentVerts[0];
    DTileCoord end = newSegment.segmentVerts.back();
    const bool place = tryPlaceRoadInternal(settlement, std::move(newSegment));
    helperAddVisLogLineBetweenCoords(mCurrentVisLog, start, end, &mWorld, place ? color::LightGreen : color::Gray);
    return place;
}

bool SettlementRoadNetwork::simpleTraceAgainstSolidRoadSegments(f32v2 start, f32v2 end) {
    for (RoadSegmentID id = 0; id < mRoadSegments.size(); ++id) {
        IntersectionHit2D hit = simpleTraceAgainstSolidRoadSegment(start, end, mRoadSegments[id]);
        if (hit.didHit()) {
            return true;
        }
    }
    return false;
}

IntersectionHit2D SettlementRoadNetwork::simpleTraceAgainstSolidRoadSegment(f32v2 start, f32v2 end, const RoadSegment& segment) {
    IntersectionHit2D hit;
    i32v2 t1 = segment.segmentVerts[0].v;
    i32v2 t2 = segment.segmentVerts.back().v;
    switch (segment.segmentType) {
        case RoadSegmentType::Segment:
            hit = IntersectionUtil::segmentSegmentIntersect(start, end, t1, t2);
            if (hit.didHit()) {
                return hit;
            }
            break;
        case RoadSegmentType::PositiveRay:
            hit = IntersectionUtil::segmentRayIntersect(start, end, t1, t2 - t1);
            if (hit.didHit() && hit.timeTarget <= 1.0f) {
                return hit;
            }
            break;
        case RoadSegmentType::NegativeRay:
            hit = IntersectionUtil::segmentRayIntersect(start, end, t2, t1 - t2);
            if (hit.didHit() && hit.timeTarget <= 1.0f) {
                return hit;
            }
            break;
        case RoadSegmentType::InfiniteLine:
            hit = IntersectionUtil::segmentLineIntersect(start, end, t1, t2 - t1);
            if (hit.didHit() && hit.timeTarget >= 0.0f && hit.timeTarget <= 1.0f) {
                return hit;
            }
            break;
        default:
            assert(false);
            break;

    }
    return IntersectionHit2D();
}

RoadSegmentIntersectBestHits SettlementRoadNetwork::getBestRoadSegmentHitsForNewPlacement(DTileCoord start, f32v2 dir) {
    constexpr f32 MAX_INFINITE_DISTANCE = 200.0f;
    f32v2 closestSegmentTimeEachDir(FLT_MAX, FLT_MAX);
    f32v2 closestInfiniteTimeEachDir(FLT_MAX, FLT_MAX);
    RoadSegmentIntersectBestHits bestHits;
    IntersectionHit2D hit;
    // Helper
    auto checkIsBestHit = [&](RoadSegmentID id) {
        if (hit.didHit()) {
            if (isInfiniteTime(hit.timeTarget)) {
                RoadSegment& hitSegment = mRoadSegments[id];
                if (hit.timeSource < 0) {
                    if (-hit.timeSource < closestInfiniteTimeEachDir.x && -hit.timeSource * hitSegment.length < MAX_INFINITE_DISTANCE) {
                        closestInfiniteTimeEachDir.x = -hit.timeSource;
                        bestHits.negativeInfiniteHit = RoadSegmentHitResult{ .hitSegmentId = id, .timeSource = hit.timeSource, .timeTarget = hit.timeTarget };
                    }
                }
                else {
                    if (hit.timeSource < closestInfiniteTimeEachDir.y && (hit.timeSource - 1.0f) * hitSegment.length < MAX_INFINITE_DISTANCE) {
                        closestInfiniteTimeEachDir.y = hit.timeSource;
                        bestHits.positiveInfiniteHit = RoadSegmentHitResult{ .hitSegmentId = id, .timeSource = hit.timeSource, .timeTarget = hit.timeTarget };
                    }
                }
            }
            else {
                // Clamp to tip to prevent nubs
                if (hit.timeTarget > 0.85f) {
                    hit.timeTarget = 1.0f;
                }
                else if (hit.timeTarget < 0.15f) {
                    hit.timeTarget = 0.0f;
                }
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
    for (RoadSegmentID id = 0; id < mRoadSegments.size(); ++id) {
        RoadSegment& segment = mRoadSegments[id];
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

bool SettlementRoadNetwork::tryPlaceRoadInternal(entt::entity settlement, RoadSegment&& newSegment) {
    // Assume bounds have been checked
    constexpr ui32 POINT_COUNT = 4;

    const RoadSegmentID newSegmentId = mRoadSegments.size();

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
        verts[i].x += mRandomGenerator.getRandomIntInRange(-2, 2);
        verts[i].y += mRandomGenerator.getRandomIntInRange(-2, 2);
    }

    const i32AABB2 aabb = getAABBFromRoadSegment(newSegment.widthTiles[0], newSegment.widthTiles[1], startVertex, endVertex);

    // Loop through the AABB and check for if it is owned already
    OwnershipGrid& ownerGrid = mWorld.getOwnershipGrid();
    IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    TerrainSurfaceGrid& roadGrid = mWorld.getTerrainSurfaceGrid();

    const i32v2 maxCoord = aabb.pos + aabb.dims;

    std::vector<RoadPointNeedingConstruct> roadVertsThisEdge;
    roadVertsThisEdge.reserve(128);

    std::unordered_set<DTileCoord> coveredPlotSeeds;
    coveredPlotSeeds.reserve(32);

    const f32 baseWidthf(newSegment.widthTiles[0]);
    const f32 endWidthf(newSegment.widthTiles[1]);

    constexpr f32 BLEND_THICKNESS = 1.0f;
    constexpr f32 MIN_ROAD_DIST = SQ(1.5f);
    DTileCoord pos;
    for (pos.y = aabb.pos.y; pos.y < maxCoord.y; ++pos.y) {
        for (pos.x = aabb.pos.x; pos.x < maxCoord.x; ++pos.x) {

            auto [closestSq, closestT] = MathUtil::computePointToLineSegmentDistanceSQAndT(pos.v, verts[0].v, verts[1].v);
            for (int i = 1; i < verts.size() - 1; ++i) {
                auto [distanceSq, time] = MathUtil::computePointToLineSegmentDistanceSQAndT(pos.v, verts[i].v, verts[i + 1].v);
                if (distanceSq < closestSq) {
                    closestSq = distanceSq;
                    closestT = time;
                }
            }

            const f32 desiredThickness = lerp(baseWidthf, endWidthf, closestT) * 0.5f;
            if (closestSq > SQ(desiredThickness)) {
                continue;
            }

            if (heightGrid.getHeightAtVert<true>(pos) <= 0.0f) {
                // If this is too close to road center, cancel the road
                if (closestSq < MIN_ROAD_DIST) {
                    return false;
                }
                continue;
            }

            // Get distance to line segment and check if its too close.
            // TODO: A road
            if (const DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerData(pos)) {
                // If there is a road owned here
                if (ownerData->owner != entt::null) {
                    // Road is invalid if it is too close to another road that is not connected to our root vertex
                    if (closestSq < MIN_ROAD_DIST) {
                        if (ownerData->owner == settlement) {
                            // TODO: Instead of just overlapping every road edge, we should be smarter...
                            //       Roads should merge and stuff
                            if (ownerData->ownerObjectType == DTileOwnerObjectType::RoadPlotSeed) {
                                coveredPlotSeeds.emplace(pos);
                            }
                            else if (!(ownerData->ownerObjectType == DTileOwnerObjectType::RoadEdge || ownerData->ownerObjectType == DTileOwnerObjectType::ExternalRoadBlocked)) {
                                return false;
                            }
                        }
                        else {
                            return false;
                        }
                    }
                }
            }
            const f32 distance = sqrtf(closestSq);
            const f32 strength = glm::min((desiredThickness - distance) / BLEND_THICKNESS, 1.0f);
            roadVertsThisEdge.emplace_back(RoadPointNeedingConstruct{ pos, strength });
        }
    }

    newSegment.roadPointsNeedingConstruct = std::move(roadVertsThisEdge);
    newSegment.roadPointsNeedingConstruct.shrink_to_fit();

    // Connect new road to other segments and block infinite edges
    constexpr f32 DOT_THRESHOLD = 0.93969262078; // cos(20) degrees is the threshold for blocking an infinite edge
    for (RoadSegmentID id = 0; id < mRoadSegments.size(); ++id) {
        RoadSegment& otherSegment = mRoadSegments[id];
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

    // Remove plot seeds we are covering
    for (DTileCoord coveredSeed : coveredPlotSeeds) {
        mPlotManager->removePlotSeed(coveredSeed);
    }

    std::unordered_set<PlotSeed> possiblePlotSeeds;
    possiblePlotSeeds.reserve(newSegment.length * (glm::max(newSegment.widthTiles[0], newSegment.widthTiles[1])) + 1);

    // Used for determining which direction to branch
    // [-pi, pi]
    const f32 baseAngle = MathUtil::yawFromDirection(newSegment.direction);
    const f32v2 rightDir = MathUtil::rotateVector2DRad(newSegment.direction, M_PI_2F);

    auto getPlotSeedDir = [rightDir, &verts, baseAngle](DTileCoord c) -> PlotSeedDir{
        const f32v2 offsetFromBase = c.v - verts[0].v;
        if (glm::dot(offsetFromBase, rightDir) > 0.0f) {
            // Right
            if (baseAngle < -M_PI_2F) {
                return PlotSeedDir::SouthEast;
            }
            else if (baseAngle < 0.0f) {
                return PlotSeedDir::NorthEast;
            }
            else if (baseAngle > M_PI_2F) {
                return PlotSeedDir::SouthWest;
            }
            return PlotSeedDir::NorthWest;
        }
        else {
            // Left
            if (baseAngle < -M_PI_2F) {
                return PlotSeedDir::NorthWest;
            }
            else if (baseAngle < 0.0f) {
                return PlotSeedDir::SouthWest;
            }
            else if (baseAngle > M_PI_2F) {
                return PlotSeedDir::NorthEast;
            }
            return PlotSeedDir::SouthEast;
        }
    };

    // Set ownership and track possible plot seeds
    for (RoadPointNeedingConstruct p : newSegment.roadPointsNeedingConstruct) {
        const DTileCoord down = p.pos - DTileCoord(0, 1);
        const DTileCoord left = p.pos - DTileCoord(1, 0);
        const DTileCoord right = p.pos + DTileCoord(1, 0);
        const DTileCoord up = p.pos + DTileCoord(0, 1);
        possiblePlotSeeds.emplace(PlotSeed{ down, getPlotSeedDir(down) }); // Down
        possiblePlotSeeds.emplace(PlotSeed{ left, getPlotSeedDir(left) }); // Left
        possiblePlotSeeds.emplace(PlotSeed{ right, getPlotSeedDir(right) }); // Right
        possiblePlotSeeds.emplace(PlotSeed{ up, getPlotSeedDir(up) }); // Up

        if (const DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerData(p.pos)) {
            // If not already owned by a road, own it with this road
            if (ownerData->ownerObjectType != DTileOwnerObjectType::RoadEdge) {
                ownerGrid.setDTileOwner(p.pos, settlement, DTileOwnerObjectType::RoadEdge, newSegmentId);
            }
        }
        else {
            ownerGrid.setDTileOwner(p.pos, settlement, DTileOwnerObjectType::RoadEdge, newSegmentId);
        }
    }

    // Set plot seeds
    for (PlotSeed s : possiblePlotSeeds) {
        if (const DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerData(s.pos)) {
            if (ownerData->ownerObjectType == DTileOwnerObjectType::None) {
                ownerGrid.setDTileOwner(s.pos, settlement, DTileOwnerObjectType::RoadPlotSeed, newSegmentId);
                mPlotManager->addPlotSeed(s.pos, newSegment.zone, s.dir);
            }
        }
        else {
            ownerGrid.setDTileOwner(s.pos, settlement, DTileOwnerObjectType::RoadPlotSeed, newSegmentId);
            mPlotManager->addPlotSeed(s.pos, newSegment.zone, s.dir);
        }
    }

    // Block other external roads and place ours
    refreshExternalRoadsInternal(newSegment, settlement);

    // ==================== BEGIN DEBUG ====================
    // TODO: REMOVE ***DEBUG BUILD ROADS***
    SimChunkTileGrid& tileGrid = mWorld.getSimTileGrid();
    for (RoadPointNeedingConstruct p : newSegment.roadPointsNeedingConstruct) {
        if (roadGrid.setBaseSurfaceTypeIfEmpty(p.pos, newSegment.roadType)) {
            // Clear tile if needed
            TileCoord tCoordsThisDTile[4];
            p.pos.getCoveredTileCoords(tCoordsThisDTile);
            for (int i = 0; i < 4; ++i) {
                if (SimTileDataWriteReservationPtr writeLock = tileGrid.tryReserveTileDataAtPosIfNotEmpty(tCoordsThisDTile[i])) {
                    writeLock->reservedCopy.tileId = TILE_ID_NONE;
                }
            }
        }
    }
    std::vector<RoadPointNeedingConstruct>().swap(newSegment.roadPointsNeedingConstruct);
    // ==================== END DEBUG ====================
    mRoadSegments.emplace_back(std::move(newSegment));
    return true;
}

void SettlementRoadNetwork::updateRoadSegmentType(RoadSegment& segment) {
    if (segment.infiniteEdges[0] == false) {
        if (segment.infiniteEdges[1] == false) {
            segment.segmentType = RoadSegmentType::Segment;
        }
        else {
            segment.segmentType = RoadSegmentType::PositiveRay;
        }
    }
    else if (segment.infiniteEdges[1] == false) {
        segment.segmentType = RoadSegmentType::NegativeRay;
    }
    else [[unlikely]] {
        segment.segmentType = RoadSegmentType::InfiniteLine;
    }
}

void SettlementRoadNetwork::refreshExternalRoadsInternal(RoadSegment& newSegment, entt::entity settlement) {
    constexpr f32 CAST_DISTANCE = 2000.0f;
    OwnershipGrid& ownerGrid = mWorld.getOwnershipGrid();
    // Helper
    auto clearBlocked = [this, &ownerGrid, &newSegment](RoadSegmentID segmentId, int index) {
        // Remove all blocked edges
        auto it = mExternalRoadSegmentBlockedTiles.find(segmentId);
        assert(it != mExternalRoadSegmentBlockedTiles.end());
        for (ExternalRoadSegmentBlockedTile& blockedTile : it->second[index]) {
            DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerDataForEditSimThread(blockedTile.pos);
            assert(ownerData);
            if (ownerData->ownerObjectType == DTileOwnerObjectType::ExternalRoadBlocked) {
                if (--ownerData->userData == 0) {
                    // TODO: This can rarely result in seeds not being restored if we have a ref count,
                    // probably fine...
                    if (blockedTile.wasPlotSeed) {
                        ownerData->ownerObjectType = DTileOwnerObjectType::RoadPlotSeed;
                        // TODO: This can parent plot seeds from other roads onto us, but thats honestly probably OK?
                        ownerData->userData = segmentId;
                        // TODO: Select the dir and segmentId based on nearby road!
                        mPlotManager->addPlotSeed(blockedTile.pos, newSegment.zone, PlotSeedDir::SouthEast /*TODO: WRONG*/);
                    }
                    else {
                        ownerData->ownerObjectType = DTileOwnerObjectType::None;
                    }
                }
            }
        }
        it->second[index].clear();
    };
    // Block any other external roads with new road
    for (auto it = mExternalRoadSegments.begin(); it != mExternalRoadSegments.end();) {
        auto& [segmentId, edges] = *it;
        RoadSegment& externalSegment = mRoadSegments[segmentId];
        if (edges.first) {
            // Negative
            f32v2 v1 = f32v2(externalSegment.segmentVerts[0].v) - externalSegment.direction * 0.5f; // Subtract small padding to ignore intersecting roads at tip
            f32v2 v2 = externalSegment.direction * -CAST_DISTANCE + v1;
            IntersectionHit2D hit = simpleTraceAgainstSolidRoadSegment(v1, v2, newSegment);
            if (hit.didHit()) {
                edges.first = false;
                clearBlocked(segmentId, 0);
                continue;
            }
        }
        if (edges.second) {
            // Positive
            f32v2 v1 = f32v2(externalSegment.segmentVerts.back().v) + externalSegment.direction * 0.5f; // Add small padding to ignore intersecting roads at tip
            f32v2 v2 = externalSegment.direction * CAST_DISTANCE + v1;
            IntersectionHit2D hit = simpleTraceAgainstSolidRoadSegment(v1, v2, newSegment);
            if (hit.didHit()) {
                edges.second = false;
                clearBlocked(segmentId, 1);
                continue;
            }
        }
        if (!edges.first && !edges.second) {

            it = mExternalRoadSegments.erase(it);
        }
        else {
            ++it;
        }
    }

    // Check if new road is external
    std::pair<bool, bool> external = { false, false };
    // Negative
    if (!simpleTraceAgainstSolidRoadSegments(f32v2(newSegment.segmentVerts[0].v), f32v2(newSegment.segmentVerts[0].v) - newSegment.direction * CAST_DISTANCE)) {
        external.first = true;
    }
    // Positive
    if (!simpleTraceAgainstSolidRoadSegments(f32v2(newSegment.segmentVerts.back().v), f32v2(newSegment.segmentVerts.back().v) + newSegment.direction * CAST_DISTANCE)) {
        external.second = true;
    }

    // Helper
    auto addBlockedRoad = [this, &ownerGrid, settlement](std::vector<ExternalRoadSegmentBlockedTile>& blockedList, ui8 width, DTileCoord startPos, DTileCoord endPos, RoadSegmentID segmentId) {
        const i32AABB2 aabb = getAABBFromRoadSegment(width, width, startPos, endPos);
        const i32v2 maxCoord = aabb.pos + aabb.dims;
        const f32 desiredThickness = f32(width) * 0.5f + 1.0f; // Add a little extra
        DTileCoord pos;
        for (pos.y = aabb.pos.y; pos.y < maxCoord.y; ++pos.y) {
            for (pos.x = aabb.pos.x; pos.x < maxCoord.x; ++pos.x) {
                auto [closestSq, closestT] = MathUtil::computePointToLineSegmentDistanceSQAndT(pos.v, startPos.v, endPos.v);
                if (closestSq <= SQ(desiredThickness)) {
                    if (DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerDataForEditSimThread(pos)) {
                        
                        if (ownerData->owner != settlement) {
                            if (ownerData->owner != entt::null) {
                                continue;
                            }
                            ownerData->owner = settlement;
                        }
                        if (ownerData->ownerObjectType == DTileOwnerObjectType::ExternalRoadBlocked) {
                            // Refcount
                            ++ownerData->userData;
                            blockedList.emplace_back(ExternalRoadSegmentBlockedTile{ pos, false });
                        }
                        else if (ownerData->ownerObjectType == DTileOwnerObjectType::RoadPlotSeed) {
                            mPlotManager->removePlotSeed(pos);
                            blockedList.emplace_back(ExternalRoadSegmentBlockedTile{ pos, true });
                            ownerData->ownerObjectType = DTileOwnerObjectType::ExternalRoadBlocked;
                            ownerData->userData = 1;
                        }
                        else if (ownerData->ownerObjectType == DTileOwnerObjectType::None) {
                            ownerGrid.setDTileOwner(pos, settlement, DTileOwnerObjectType::ExternalRoadBlocked, 1);
                            blockedList.emplace_back(ExternalRoadSegmentBlockedTile{ pos, false });
                        }
                    }
                    else {
                        ownerGrid.setDTileOwner(pos, settlement, DTileOwnerObjectType::ExternalRoadBlocked, 1);
                    }
                }
            }
        }
    };

    constexpr f32 BLOCK_CAST_DISTANCE = 30.0f;
    if (external.first || external.second) {
        // New segment has not been added yet so its ID is the road segments list size
        const RoadSegmentID newSegmentId = mRoadSegments.size();
        mExternalRoadSegments.emplace(newSegmentId, external);
        auto& blockedTilesList = mExternalRoadSegmentBlockedTiles[newSegmentId];

        if (external.first) {
            const DTileCoord startPos = newSegment.segmentVerts[0];
            const DTileCoord endPos = DTileCoord(i32v2(glm::round(f32v2(startPos.v) - newSegment.direction * BLOCK_CAST_DISTANCE)));
            addBlockedRoad(blockedTilesList[0], newSegment.widthTiles[0], startPos, endPos, newSegmentId);
        }
        if (external.second) {
            const DTileCoord startPos = newSegment.segmentVerts.back();
            const DTileCoord endPos = DTileCoord(i32v2(glm::round(f32v2(startPos.v) + newSegment.direction * BLOCK_CAST_DISTANCE)));
            addBlockedRoad(blockedTilesList[1], newSegment.widthTiles[1], startPos, endPos, newSegmentId);
        }
    }
}
