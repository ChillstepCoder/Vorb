#include "stdafx.h"
#include "SettlementLayoutManager.h"

#include "world/World.h"
#include "world/ownership/OwnershipGrid.h"
#include "world/road/TerrainSurfaceGrid.h"
#include "world/IHeightmapGrid.h"
#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/component/SimSettlementComponents.h"

#include "debugging/DebugRenderer.h"
#include "debugging/VisualLogger.h"

#include "world/settlement/SettlementDebugHelpers.inl"

#include "world/settlement/SettlementPlotManager.h"
#include "world/settlement/SettlementRoadNetwork.h"
#include "building/BuildingGrid.h"

#include "building/buildingBlueprintGenerator.h"
#include "building/buildingRepository.h"

SettlementLayoutManager::SettlementLayoutManager() = default;
SettlementLayoutManager::~SettlementLayoutManager() = default;

SettlementLayoutManager::SettlementLayoutManager(SettlementLayoutManager&& o) = default;
SettlementLayoutManager& SettlementLayoutManager::operator=(SettlementLayoutManager&& o) = default;

constexpr f32 RURAL_SPAWN_RADIUS = 140.0f;
constexpr f32 INITIAL_GOVERNMENT_RADIUS = 40.f;
constexpr f32 RURAL_SPACING = 20.0f;
constexpr f32 URBAN_SPACING = 7.0f;
constexpr f32 COMMERCIAL_FREQ = M_PIF * 3.0f; // USE WHOLE NUMBER
constexpr f32 COMMERCIAL_CUTOFF = -0.1f; // Larger number = more residential

bool SettlementLayoutManager::tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos) {

    mSettlementOrientation = MathUtil::rotateVector2DRad(f32v2(1.0f, 0.0f), mRandomGenerator.getRandomFloatUnsigned() * glm::two_pi<f32>());

    mRoadNetwork = std::make_unique<SettlementRoadNetwork>(world, mRandomGenerator);
    mPlotManager = std::make_unique<SettlementPlotManager>(world, mRandomGenerator);
    mRoadNetwork->init(*mPlotManager);

    VisualLog* visLog = VisualLogger::tryGetNewVisualLog("Settlement: " + std::to_string(dTilePos.v.x) + "," + std::to_string(dTilePos.v.y), VisualLogCategory::Settlement, true);
    if (visLog) {
        const TileCoord tPos(dTilePos);
        visLog->setCameraDistanceCheckPosOffset(f32v3(tPos.x, tPos.y, 0.0f));
        mCurrentVisLog = visLog;
        mRoadNetwork->mCurrentVisLog = visLog;
    }

    mWorld = &world;
    mSettlementEntity = settlement;
    mSimEcs = mWorld->tryGetSimECS();
    assert(mSimEcs);
    mRootPos = dTilePos;
    assert(mSectors.empty());
    mRandomGenerator.setSeed(RandomGenerator::DEFAULT_SEED * (ui32)settlement + dTilePos.x ^ dTilePos.y);
    mSectors.reserve(64);
    mOpenSectors.reserve(64);
    mRoadNetwork->mRoadSegments.reserve(64);


    //constexpr f32 DESIRED_RADIUS = INITIAL_GOVERNMENT_RADIUS;
    //constexpr f32 PADDED_RADIUS = DESIRED_RADIUS + 1.0f;

    if (!tryAddSector(dTilePos, INITIAL_GOVERNMENT_RADIUS, SettlementZone::Government)) [[unlikely]] {
        panic("Failed to add first sector for settlement");
    }
    // Try many times to add our second sector, should return first time, but could be blocked
    bool didAddSecondSector = false;
    for (int i = 0; i < 32; ++i) {
        if (tryAddNewRandomSector(SettlementZone::ALL) != SettlementZone::INVALID) {
            didAddSecondSector = true;
            break;
        }
    }
    if (!didAddSecondSector) {
        return false;
    }

    //debugInitSettlementPartiallyMade();

    if (mCurrentVisLog) {
        visLog->finish();
        mCurrentVisLog = nullptr;
        mRoadNetwork->mCurrentVisLog = nullptr;
    }
    return true;
}

SettlementZone SettlementLayoutManager::tryAddNewRandomSector(BitFlags<SettlementZone> allowedZones) {
    assert(mSectors.size());

    // Just to determine direction
    constexpr f32 initialRadius = 32.f;
    constexpr ui32 TRY_COUNT = 8;
    for (ui32 i = 0; i < TRY_COUNT; ++i) {
        const ui32 sectorIndex = mRandomGenerator.getRandomUIntInRange(0, mSectors.size());
        const SettlementSector& sector = mSectors[sectorIndex];
        const f32 initialDistance = sector.desiredRadius + initialRadius;
        const f32 angle = mRandomGenerator.getRandomFloatUnsigned() * glm::two_pi<f32>();
        const f32v2 offset = MathUtil::getNormalVectorFromAngleRad(angle) * initialDistance;
        DTileCoord newPos = sector.center + DTileCoord(i32v2(glm::round(offset)));
        // Second zone must always be government
        SettlementZone desiredZone = SettlementZone::Government;
        f32 desiredRadius = INITIAL_GOVERNMENT_RADIUS;
        if (mSectors.size() > 1) [[likely]] {
            auto p = getDesiredZoneAndRadiusAtCoord(newPos);
            desiredZone = p.first;
            desiredRadius = p.second;
        }
        // Adjust distance
        const f32 distanceAdjustScale = (sector.desiredRadius + desiredRadius + 1.0f) / initialDistance;
        // Recalculate pos
        newPos = sector.center + DTileCoord(i32v2(glm::round(offset * distanceAdjustScale)));
        if (allowedZones.isBitSet(desiredZone)) {
            if (tryAddSector(newPos, desiredRadius, desiredZone)) {
                return desiredZone;
            }
        }
    }
    return SettlementZone::INVALID;
}

bool SettlementLayoutManager::tryAddSector(DTileCoord center, f32 desiredRadius, SettlementZone zone) {

    { // Handle ownership
        ChunkCoord chunkPos(center);
        OwnershipGrid& ownerGrid = mWorld->getOwnershipGrid();
        ChunkID chunkId = chunkPos.toGridIDType(mWorld->getWidthChunks());
        const entt::entity prevChunkOwner = ownerGrid.getChunkSettlementOwner(chunkId);
        if (prevChunkOwner == entt::null) {
            ownerGrid.setChunkOwner(chunkId, mSettlementEntity);
        }
        else if (prevChunkOwner != mSettlementEntity) {
            return false;
        }
    }

    if (mSectors.empty()) [[unlikely]] {
        mSectors.emplace_back(center, desiredRadius, mSectors.size(), zone);
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

    SettlementSector newSector(center, desiredRadius, mSectors.size(), zone);

    constexpr ui32 DESIRED_ROAD_WIDTH = 5;

    // Create a road between new sector and closest other sectors
    auto it = sectorDistances.begin();
    constexpr ui32 MAX_ROADS_ADDED = 3;
    bool didAddRoad = false;
    for (ui32 i = 0; i < MAX_ROADS_ADDED && it != sectorDistances.end(); ++i, ++it) {
        const SettlementSector& otherSector = mSectors[it->second];
        if (!mRoadNetwork->simpleTraceAgainstSolidRoadSegments(newSector.center.v, otherSector.center.v)) {
            helperAddVisLogLineBetweenCoords(mCurrentVisLog, newSector.center, otherSector.center, mWorld, color::Green);
            // Clear line of sight to other sector, now try making a road
            const f32 distanceRatio = newSector.desiredRadius / (newSector.desiredRadius + otherSector.desiredRadius);
            didAddRoad |= mRoadNetwork->tryAddRoadBetweenSectorPoints(
                mSettlementEntity,
                newSector.center,
                otherSector.center,
                DTileCoord(i32v2(glm::round(vmath::lerp(f32v2(newSector.center.v), f32v2(otherSector.center.v), distanceRatio)))),
                TerrainSurfaceType::DirtRoad,
                DESIRED_ROAD_WIDTH,
                zone
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
    AM::DebugRenderer::drawWireQuadThreadSafe(rootWorldPos - WIDTH * 0.5f, WIDTH, color::Green, FRAME_COUNT);

    // Chunk borders
    const auto& ownedChunks = registry.get<ChunkOwnershipComponent>(mSettlementEntity).ownedChunks;
    constexpr f32 DEFAULT_Z = 1.0f;
    constexpr i32 chunkRowLengthDTiles = DTileCoord::getRowLengthPerChunk();
    AM::DebugRenderer::reserveLinesThreadSafe(SQ(chunkRowLengthDTiles) * ownedChunks.size() * 4, FRAME_COUNT);
    for (ui32 chunkId : ownedChunks) {
        TileCoord worldPos = mWorld->getChunkWorldPos(chunkId);
        DTileCoord dTileCoord(worldPos);
        AM::DebugRenderer::drawWireQuadThreadSafe(f32v3(worldPos.v.x, worldPos.v.y, DEFAULT_Z), f32v2(CHUNK_WIDTH), color4(1.0f, 1.0f, 1.0f, 0.5f), FRAME_COUNT);
        // Debug ownership types
        for (i32 y = 0; y < chunkRowLengthDTiles; ++y) {
            for (i32 x = 0; x < chunkRowLengthDTiles; ++x) {
                DTileCoord coord(dTileCoord.v + i32v2(x, y));
                const DTileOwnershipData* dtileOwnerData = ownerGrid.tryGetDTileOwnerData(coord);
                if (dtileOwnerData && dtileOwnerData->ownerObjectType != DTileOwnerObjectType::None && dtileOwnerData->owner == mSettlementEntity) {
                    const i32v4 aabb = coord.toTileAABBRound();
                    f32v3 aabbWorldPos = helperGetWorldPosFrom2DPos(f32v2(aabb.x, aabb.y), mWorld);
                    aabbWorldPos.z -= 1.0f;
                    color4 dcolor = color::White;
                    switch (dtileOwnerData->ownerObjectType) {
                        case DTileOwnerObjectType::Plot:
                            dcolor = color::LawnGreen;
                            break;
                        case DTileOwnerObjectType::RoadEdge: {
                            const RoadSegment& segment = mRoadNetwork->mRoadSegments[dtileOwnerData->userData];
                            dcolor = getSettlementZoneDebugColor(segment.zone);
                            break;
                        }
                        case DTileOwnerObjectType::RoadPlotSeed: {
                            const PlotSeed seed = mPlotManager->getPlotSeed(coord);
                            switch (seed.dir) {
                                case PlotSeedDir::SouthWest:
                                    dcolor = color::Blue;
                                    break;
                                case PlotSeedDir::SouthEast:
                                    dcolor = color::Red;
                                    break;
                                case PlotSeedDir::NorthWest:
                                    dcolor = color::LightBlue;
                                    break;
                                case PlotSeedDir::NorthEast:
                                    dcolor = color::Pink;
                                    break;
                                case PlotSeedDir::NONE:
                                    dcolor = color::Black;
                                    break;
                                default:
                                    break;
                            }
                            break;
                        }
                        case DTileOwnerObjectType::Structure:
                            dcolor = color::DarkGreen;
                            break;
                        case DTileOwnerObjectType::ExternalRoadBlocked:
                            dcolor = color::DarkGray;
                            break;
                        default:
                            break;
                    }
                    static_assert(e_count(DTileOwnerObjectType) == 6);

                    AM::DebugRenderer::drawWireQuadThreadSafe(aabbWorldPos, f32v2(aabb.z, aabb.w), dcolor, FRAME_COUNT);
                }
            }
        }
    }

    // Sector centers
    for (auto& sector : mSectors) {
        f32v3 sectorWorldPos = helperGetWorldPosFromDTileCoord(sector.center, mWorld);
        AM::DebugRenderer::drawWireQuadThreadSafe(sectorWorldPos - WIDTH * 0.5f, WIDTH, getSettlementZoneDebugColor(sector.zone), FRAME_COUNT);
    }
    
    // Roads
    ui32 i = 0;
    for (auto& segment : mRoadNetwork->mRoadSegments) {
        f32v3 worldPosA = helperGetWorldPosFromDTileCoord(segment.segmentVerts[0], mWorld);
        f32v3 worldPosB = helperGetWorldPosFromDTileCoord(segment.segmentVerts.back(), mWorld);
        f32v3 infiniteRayOffset(segment.direction.x, segment.direction.y, 0.0f);
        AM::DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosA, worldPosB, color::Yellow, FRAME_COUNT);
        if (segment.infiniteEdges[0]) {
            AM::DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosA, worldPosA - infiniteRayOffset, color::Cyan, FRAME_COUNT);
        }
        if (segment.infiniteEdges[1]) {
            AM::DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosB, worldPosB + infiniteRayOffset, color::Cyan, FRAME_COUNT);
        }
        ++i;
    }

    // External roads
    for (auto& [segmentId, edges] : mRoadNetwork->mExternalRoadSegments) {
        const RoadSegment& segment = mRoadNetwork->mRoadSegments[segmentId];
        const f32v3 infiniteRayOffset(segment.direction.x * 2.0f, segment.direction.y * 2.0f, 0.0f);
        if (edges.first) {
            const f32v3 worldPosA = helperGetWorldPosFromDTileCoord(segment.segmentVerts[0], mWorld);
            AM::DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosA, worldPosA - infiniteRayOffset, color::OrangeRed, FRAME_COUNT);
        }
        if (edges.second) {
            const f32v3 worldPosB = helperGetWorldPosFromDTileCoord(segment.segmentVerts.back(), mWorld);
            AM::DebugRenderer::drawLineBetweenPointsThreadSafe(worldPosB, worldPosB + infiniteRayOffset, color::OrangeRed, FRAME_COUNT);
        }
    }

    // Plots
    const std::vector<SettlementPlot>& plots = mPlotManager->getPlots();
    for (const SettlementPlot& plot : plots) {
        DTileCoord root(plot.aabbDTile.pos);
        const f32v3 worldPosA = helperGetWorldPosFromDTileCoord(root, mWorld);
        AM::DebugRenderer::drawWireQuadThreadSafe(worldPosA, f32v2(plot.aabbDTile.dims) * (f32)DTILE_WIDTH, color::Magenta, FRAME_COUNT);
    }
}

SettlementPlot& SettlementLayoutManager::getPlot(SettlementPlotID id) {
    return mPlotManager->getPlot(id);
}

const SettlementPlot& SettlementLayoutManager::getPlot(SettlementPlotID id) const {
    return mPlotManager->getPlot(id);
}

SettlementPlotID SettlementLayoutManager::tryClaimOrGeneratePlot(SettlementPlotRequest& request, entt::entity owner, bool allowAddSector) {
    SettlementPlotID id = mPlotManager->tryClaimOrGeneratePlot(request, owner, nullptr);
    if (id == INVALID_SETTLEMENT_PLOT_ID && allowAddSector) {
        constexpr int MAX_TRIES = 3;
        for (int i = 0; i < MAX_TRIES; ++i) {
            const SettlementZone generatedZone = tryAddNewRandomSector(request.allowedZones);
            if (generatedZone != SettlementZone::INVALID) {
                // Only looking in the new zone
                request.allowedZones.overwriteBits(generatedZone);
                return mPlotManager->tryClaimOrGeneratePlot(request, owner, nullptr);
            }
        }
    }
    return id;
}

std::pair<SettlementZone, f32> SettlementLayoutManager::getDesiredZoneAndRadiusAtCoord(DTileCoord coord) {

    const f32v2 offsetFromRoot((coord - mRootPos).v);
    const f32 offsetLength = glm::length(offsetFromRoot);
    // Beyond certain radius always rural
    if (offsetLength > RURAL_SPAWN_RADIUS) {
        return std::make_pair(SettlementZone::Rural, RURAL_SPACING);
    }

    const f32v2 offsetNormal = offsetFromRoot / offsetLength;
    const f32 dot = glm::dot(offsetNormal, mSettlementOrientation);
    // Oscillate between residential and commercial in a circle
    if (cos((dot + 1.0f) * COMMERCIAL_FREQ) >= COMMERCIAL_CUTOFF) {
        return std::make_pair(SettlementZone::UrbanResidential, URBAN_SPACING);
    }
    return std::make_pair(SettlementZone::UrbanCommercial, URBAN_SPACING);
}

void SettlementLayoutManager::debugInitSettlementPartiallyMade() {

    for (ui32 i = 0; i < 6; ++i) {
        tryAddNewRandomSector(SettlementZone::ALL);
    }
    if (mCurrentVisLog) {
        mCurrentVisLog->nextStep("Plots");
    }

    IHeightmapGrid& heightGrid = mWorld->getHeightmapGrid();
    BuildingRepository& buildingRepo = BuildingRepository::get();
    const BuildingDef& houseDef = buildingRepo.getLoadedOrUnloadedAsset(CStrToken("small_house"));

    for (ui32 i = 0; i < 32; ++i) {
        SettlementPlotRequest request;
        request.allowedZones = mRandomGenerator.getRandomBool() ? SettlementZone::Rural : SettlementZone::UrbanResidential;
        SettlementPlotID newPlotID = mPlotManager->tryGenerateNewPlot(request, mSettlementEntity, mCurrentVisLog);
        if (newPlotID != INVALID_SETTLEMENT_PLOT_ID) {
            SettlementPlot& newPlot = mPlotManager->getPlot(newPlotID);
            mPlotManager->debugMarkPlotFree(newPlotID);
            //const i32 approxZPos = heightGrid.getHeightAtVert<true>(DTileCoord(newPlot.aabbDTile.getCenter()));
            //BuildingBlueprintPtr bp = BuildingBlueprintGenerator::tryGenerateBlueprintSynchronous(houseDef, 1.0f /*?*/, Cartesian::WEST, DTileCoord(newPlot.aabbDTile.pos), newPlot.aabbDTile.dims, newPlot.ownedDTiles, BuildingBlueprintFlags(0), Random::getCachedRandom(), approxZPos);
            //if (bp) {
            //    BitArray tilesNeedingTerrainFlatten = bp->computeSolidTilesFirstFloor();

            //    // Clamp building height to 1 meter increments
            //    const i32AABB2 tileAABB(newPlot.aabbDTile.pos << 1, newPlot.aabbDTile.dims << 1);
            //    const ui32 meanHeight = round(heightGrid.computeMeanHeightAtAABB(tileAABB, tilesNeedingTerrainFlatten));
            //    const i32AABB3 aabb3d(i32v3(tileAABB.pos.x, tileAABB.pos.y, meanHeight), i32v3(tileAABB.dims.x, tileAABB.dims.y, bp->floorCount * bp->floorHeight));

            //    Building* newBuilding = static_cast<Building*>(mWorld->getBuildingGrid().tryMakeNewFullyBuiltBuilding(aabb3d, bp->floorHeight, bp->ownedDTiles, bp));
            //    // TODO: Store building reference
            //}
        }
    }
}
