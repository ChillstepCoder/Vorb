#include "stdafx.h"
#include "SettlementLayoutManager.h"

#include "world/World.h"
#include "world/ownership/OwnershipGrid.h"
#include "world/road/RoadGrid.h"
#include "world/IHeightmapGrid.h"
#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/component/SettlementComponents.h"

#include "debugging/DebugRenderer.h"
#include "debugging/VisualLogger.h"

#include "world/settlement/SettlementDebugHelpers.inl"

bool SettlementLayoutManager::tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos) {

    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("Settlement: " + std::to_string(dTilePos.v.x) + "," + std::to_string(dTilePos.v.y), VisualLogCategory::Settlement, true);
    if (visLog) {
        const TileCoord tPos(dTilePos);
        visLog->setCameraDistanceCheckPosOffset(f32v3(tPos.x, tPos.y, 0.0f));
        mCurrentVisLog = visLog;
        mRoadNetwork.mCurrentVisLog = visLog;
    }

    mWorld = &world;
    mSettlementEntity = settlement;
    mSimEcs = mWorld->tryGetSimECS();
    assert(mSimEcs);
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

    addedCount += (i32)tryAddSector(dTilePos + DTileCoord(PADDED_RADIUS, (i32)-PADDED_RADIUS), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS, (i32)PADDED_RADIUS), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)-PADDED_RADIUS), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)PADDED_RADIUS), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(-PADDED_RADIUS, (i32)PADDED_RADIUS * 3.3f), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS * 2.0f, (i32)PADDED_RADIUS * 4.3f), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS * 3.0f, (i32)PADDED_RADIUS), DESIRED_RADIUS);
    //addedCount += (i32)tryAddSector(settlement, dTilePos + DTileCoord(PADDED_RADIUS * 3.0f, (i32)-PADDED_RADIUS), DESIRED_RADIUS);
   
    for (ui32 i = 0; i < 16; ++i) {
        addedCount += tryAddNewRandomSector();
    }

    if (mCurrentVisLog) {
        visLog->finish();
        mCurrentVisLog = nullptr;
        mRoadNetwork.mCurrentVisLog = nullptr;
    }
    return addedCount > 1;
}

bool SettlementLayoutManager::tryAddNewRandomSector() {
    const f32 desiredRadius = 16.f + mRoadNetwork.randomGenerator.getRandomFloatUnsigned() * 8.0f;
    constexpr ui32 TRY_COUNT = 8;
    for (ui32 i = 0; i < TRY_COUNT; ++i) {
        const ui32 sectorIndex = mRoadNetwork.randomGenerator.getRandomUIntInRange(0, mSectors.size());
        const SettlementSector& sector = mSectors[sectorIndex];
        const f32 distance = sector.desiredRadius + desiredRadius + 2.0f;
        const f32 angle = mRoadNetwork.randomGenerator.getRandomFloatUnsigned() * glm::two_pi<f32>();
        // TODO: GetNormalDir util
        const f32v2 offset = MathUtil::rotateVector2DRad(f32v2(distance, 0.0f), angle);
        const DTileCoord newPos = sector.center + DTileCoord(i32v2(glm::round(offset)));
        if (tryAddSector(newPos, desiredRadius)) {
            return true;
        }
    }
    return false;
}

bool SettlementLayoutManager::tryAddSector(DTileCoord center, f32 desiredRadius) {

    { // Handle ownership
        ChunkCoord chunkPos(center);
        OwnershipGrid& ownerGrid = mWorld->getOwnershipGrid();
        ChunkID chunkId = chunkPos.toGridIDType(mWorld->getWidthChunks());
        const entt::entity prevChunkOwner = ownerGrid.getChunkSettlementOwner(chunkId);
        if (prevChunkOwner == entt::null) {
            ownerGrid.setChunkSettlementOwner(chunkId, mSettlementEntity);
            entt::registry& registry = mSimEcs->getRegistrySimThread();
            registry.get<SettlementDetailsComponent>(mSettlementEntity).ownedChunks.emplace_back(chunkId);
        }
        else if (prevChunkOwner != mSettlementEntity) {
            return false;
        }
    }

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
                mSettlementEntity,
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

    entt::registry& registry = mSimEcs->getRegistrySimThread();

    IHeightmapGrid& heightGrid = mWorld->getHeightmapGrid();
    OwnershipGrid& ownerGrid = mWorld->getOwnershipGrid();
    TileCoord pos(mRootPos);
    const f32 WORLD_Z = 5.0f;
    f32v3 rootWorldPos(pos.x, pos.y, WORLD_Z);
    const f32v3 WIDTH(3.0f, 3.0f, 0.0f);
    const ui32 FRAME_COUNT = 32;
    // Center
    DebugRenderer::drawWireQuadThreadSafe(rootWorldPos - WIDTH * 0.5f, WIDTH, color::Green, FRAME_COUNT);

    // Chunk borders
    const auto& ownedChunks = registry.get<SettlementDetailsComponent>(mSettlementEntity).ownedChunks;
    constexpr f32 DEFAULT_Z = 1.0f;
    constexpr i32 chunkRowLengthDTiles = DTileCoord::getRowLengthPerChunk();
    DebugRenderer::reserveLinesThreadSafe(SQ(chunkRowLengthDTiles) * ownedChunks.size() * 4, FRAME_COUNT);
    for (ui32 chunkId : ownedChunks) {
        TileCoord worldPos = mWorld->getChunkWorldPos(chunkId);
        DTileCoord dTileCoord(worldPos);
        DebugRenderer::drawWireQuadThreadSafe(f32v3(worldPos.v.x, worldPos.v.y, DEFAULT_Z), f32v2(CHUNK_WIDTH), color4(1.0f, 1.0f, 1.0f, 0.5f), FRAME_COUNT);
        // Debug ownership types
        for (i32 y = 0; y < chunkRowLengthDTiles; ++y) {
            for (i32 x = 0; x < chunkRowLengthDTiles; ++x) {
                DTileCoord coord(dTileCoord.v + i32v2(x, y));
                const DTileOwnershipData* dtileOwnerData = ownerGrid.tryGetDTileOwnerData(coord);
                if (dtileOwnerData && dtileOwnerData->ownerObjectType != DTileOwnerObjectType::None) {
                    const i32v4 aabb = coord.toTileAABB();
                    f32v3 aabbWorldPos = helperGetWorldPosFrom2DPos(f32v2(aabb.x, aabb.y), mWorld);
                    aabbWorldPos.z -= 1.0f;
                    color4 dcolor = color::White;
                    switch (dtileOwnerData->ownerObjectType) {
                        case DTileOwnerObjectType::Plot:
                            dcolor = color::LawnGreen;
                            break;
                        case DTileOwnerObjectType::RoadEdge:
                            dcolor = color::RoyalBlue;
                            break;
                        case DTileOwnerObjectType::Structure:
                            dcolor = color::DarkGreen;
                            break;
                        default:
                            break;
                    }
                    static_assert(e_count(DTileOwnerObjectType) == 4);

                    DebugRenderer::drawWireQuadThreadSafe(aabbWorldPos, f32v2(aabb.z, aabb.w), dcolor, FRAME_COUNT);
                }
            }
        }
    }

    // Sector centers
    for (auto& sector : mSectors) {
        f32v3 sectorWorldPos = helperGetWorldPosFromDTileCoord(sector.center, mWorld);
        DebugRenderer::drawWireQuadThreadSafe(sectorWorldPos - WIDTH * 0.5f, WIDTH, color::Red, FRAME_COUNT);
    }
    
    // Roads
    ui32 i = 0;
    for (auto& segment : mRoadNetwork.roadSegments) {
        color4 color = color::Yellow;
        f32v3 worldPosA = helperGetWorldPosFromDTileCoord(segment.segmentVerts[0], mWorld);
        f32v3 worldPosB = helperGetWorldPosFromDTileCoord(segment.segmentVerts.back(), mWorld);
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
