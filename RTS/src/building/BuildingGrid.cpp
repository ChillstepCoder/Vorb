#include "stdafx.h"

#include "building/Building.h"
#include "building/BuildingBlueprint.h"
#include "BuildingGrid.h"

#include "gamethread/GameThreadTasks.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimThread.h"

#include "tile/TileContainerRepository.h"

#include "world/World.h"
#include "tile/TileContainerLoader.h"

#include "debugging/DebugRenderer.h"

#include "boost/container/flat_set.hpp"
#include "pathfinding/NavWorld.h"

// HOW IT WORKS
// 1. Sim thread OR game thread create a building, gets marked as SIM state and disconnected
// 2. Building gets sent to game thread to load
// 3. Once 

// TODO: Serialize this?
BuildingID sBuildingIdGen = 0;

BuildingGrid::BuildingGrid(World& world) : mWorld(world) {
    initEventHandlers();
    mChunkBuildingData = std::make_unique<ChunkBuildingData[]>(SQ(mWorld.getWidthChunks()));
}

void BuildingGrid::tick() {
    ASSERT_GAME_THREAD();
    for (size_t i = 0; i < mDeactivatingBuildings.size();) {
        Building* bldg = mDeactivatingBuildings[i];
        assert(bldg->mIsDeactivating);
        // Wait for ref to be 0 and load to finish
        if (bldg->getRefCountTiles() == 0 && bldg->getState() == BuildingState::ACTIVE) {
            bldg->mIsDeactivating = false;
            bldg->mState = BuildingState::SIM;
            bldg->freeData();
            mDeactivatingBuildings[i] = mDeactivatingBuildings.back();
            mDeactivatingBuildings.pop_back();
        } else {
            ++i;
        }
    }
}

Building* BuildingGrid::tryMakeNewFullyBuiltBuilding(const i32AABB3& tileAABB, ui32 floorHeight, const BitArray& ownedDTiles, std::unique_ptr<BuildingBlueprint>& bptr) {
    const DTileCoord rootDTileCoord = DTileCoord::fromTilePosRound(tileAABB.pos);
    const ui32v2 DTileDims((tileAABB.dims.x >> 1), (tileAABB.dims.y >> 1));
    assert(DTileDims.x < MAX_BUILDING_WIDTH_DTILES && DTileDims.y < MAX_BUILDING_WIDTH_DTILES);

    // Cache all dtile positions pre-lock to reduce critical section load
    DTileCoord coveredTiles[SQ(MAX_BUILDING_WIDTH_DTILES)];
    ui32 coveredTilesCount = 0;
    DTileCoord iter = rootDTileCoord;
    for (iter.y = 0; iter.y < DTileDims.y; ++iter.y) {
        for (iter.x = 0; iter.x < DTileDims.x; ++iter.x) {
            if (ownedDTiles.getBit(iter.y * DTileDims.x + iter.x)) {
                coveredTiles[coveredTilesCount++] = rootDTileCoord + iter;
            }
        }
    }

    //  Helper
    auto structureExists = [this](DTileCoord dTileCoord) -> bool {
        const ChunkCoord chunkCoord(dTileCoord);
        const ChunkID chunkId = chunkCoord.toGridIDType(mWorld.getWidthChunks());

        const ChunkBuildingData& structureData = mChunkBuildingData[chunkId];
        std::lock_guard lock(structureData.mMutex);
        if (!structureData.dTileBuildings) {
            return false;
        }
        const DTileCoord dTileOffset = dTileCoord - DTileCoord(chunkCoord);
        const DTileIndex index = dTileOffset.y * CHUNK_WIDTH_DTILES + dTileOffset.x;
        const BuildingID id = structureData.dTileBuildings[index];
        if (id == INVALID_BUILDING_ID) {
            return false;
        }
        return true;
    };

    BuildingID newBuildingId;

    // First check if we can actually place structure here, and place them in one lock
    // Must be done this way to avoid any race conditions
    { // Critical section
        std::lock_guard lock(mBuildingIDMutex);
        newBuildingId = sBuildingIdGen;
        // Check
        for (ui32 di = 0; di < coveredTilesCount; ++di) {
            const DTileCoord worldCoord = coveredTiles[di];
            if (structureExists(worldCoord)) {
                return nullptr;
            }
        }

        // Allocate
        newBuildingId = sBuildingIdGen++;
    }

    std::unique_ptr<Building> newBuilding = std::make_unique<Building>();
    assert(ownedDTiles.getNumBits() >= DTileDims.x * DTileDims.y);
    i32v3 tileDims = tileAABB.dims;
    assert((tileDims.z % floorHeight) == 0);
    tileDims.z /= floorHeight;
    assert(tileDims.x < CHUNK_WIDTH && tileDims.y < CHUNK_WIDTH);
    newBuilding->mOwnedDTiles = ownedDTiles;
    newBuilding->mTileAABB = tileAABB;
    newBuilding->mId = newBuildingId;
    newBuilding->setBlueprint(std::move(bptr));

    // Hook up chunk dependencies
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    i32v2 worldXY;
    boost::container::flat_set<Chunk*> chunkDependencies;
    chunkDependencies.reserve(4);
    worldXY = i32v2(tileAABB.x, tileAABB.y);
    chunkDependencies.insert(&chunkGrid.getChunkAtPosition(worldXY));
    worldXY = i32v2(tileAABB.x + tileAABB.dims.x, tileAABB.y);
    chunkDependencies.insert(&chunkGrid.getChunkAtPosition(worldXY));
    worldXY = i32v2(tileAABB.x, tileAABB.y + tileAABB.dims.y);
    chunkDependencies.insert(&chunkGrid.getChunkAtPosition(worldXY));
    worldXY = i32v2(tileAABB.x + tileAABB.dims.x, tileAABB.y + tileAABB.dims.y);
    chunkDependencies.insert(&chunkGrid.getChunkAtPosition(worldXY));

    assert(chunkDependencies.size() && chunkDependencies.size() <= 4);
    int chunkCount = 0;
    for (auto&& c : chunkDependencies) {
        // Chunks increment our refcount while they are loaded
        newBuilding->mChunkDependencies[chunkCount++] = c->getChunkID();
    }
    newBuilding->mChunkDependencyCount = chunkCount;
    newBuilding->mChunkDependenciesActiveCount = 0;
    assert(floorHeight < UINT8_MAX); 
    newBuilding->mFloorHeight = floorHeight;

    Building* rv = newBuilding.get();
    
    // Track in list
    {
        std::lock_guard lock(mBuildingsMutex);
        mBuildings[newBuilding->mId] = std::move(newBuilding);
    }

    // Hook up to chunk dtile grids
    for (ui32 di = 0; di < coveredTilesCount; ++di) {
        const DTileCoord worldCoord = coveredTiles[di];
        const ChunkCoord chunkCoord(worldCoord);
        const ChunkID chunkId = chunkCoord.toGridIDType(mWorld.getWidthChunks());
        ChunkBuildingData& structureData = mChunkBuildingData[chunkId];
        std::lock_guard lock(structureData.mMutex);
        // Initialize structure data if needed
        if (!structureData.dTileBuildings) [[unlikely]] {
            structureData.dTileBuildings = std::make_unique<BuildingID[]>(CHUNK_SIZE_DTILES);
            for (ui32 i = 0; i < CHUNK_SIZE_DTILES; ++i) {
                structureData.dTileBuildings[i] = INVALID_BUILDING_ID;
            }
        }
        const DTileCoord dTileOffset = worldCoord - DTileCoord(chunkCoord);
        const DTileIndex index = dTileOffset.y * CHUNK_WIDTH_DTILES + dTileOffset.x;
        structureData.dTileBuildings[index] = newBuildingId;
    }

    // Flatten terrain and initialize building on main thread
    GameThreadTasks::getInstance().addGenericTask([this, newBuilding = rv, tileAABB]() {
        IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
        BuildingBlueprint& bp = *newBuilding->getBlueprint();
        const i32v2 worldPos = bp.worldPosRootDTile.toTilePos();
        const i32 floorStride = tileAABB.dims.x * tileAABB.dims.y;
        for (ui32 i = 0; i < bp.tileTargetCount; ++i) {
            BuildingBlueprintTileTarget& tileTarget = bp.tileTargets[i];
            // Footprint
            if (tileTarget.tileIndex < floorStride) {
                i32v2 tileWorldPos = worldPos + i32v2(tileTarget.tileIndex % tileAABB.dims.x, tileTarget.tileIndex / tileAABB.dims.x);
                // Epsilon to prevent z fighting
                heightGrid.setHeightAtWorldPos(tileWorldPos, tileAABB.z - 0.005f);
                // Mark covered
                ChunkCoord chunkc = ChunkCoord::fromTilePos(tileWorldPos);
                ChunkID chunkId = chunkc.toGridIDType(mWorld.getWidthChunks());
                mChunkBuildingData[chunkId].setTileCoveredByBuilding((tileWorldPos.y % CHUNK_WIDTH) * CHUNK_WIDTH + tileWorldPos.x % CHUNK_WIDTH);
            }
            else {
                break; // Only need to iterate first floor, BP is sorted
            }
        }
        for (ui32 i = 0; i < bp.stairPieceCount; ++i) {
            StairPiece& stairPiece = bp.stairPieces[i];
            if (stairPiece.pos < floorStride) {
                if (stairPiece.pos < floorStride) {
                    i32v2 tileWorldPos = worldPos + i32v2(stairPiece.pos % tileAABB.dims.x, stairPiece.pos / tileAABB.dims.x);
                    // Epsilon to prevent z fighting
                    heightGrid.setHeightAtWorldPos(tileWorldPos, tileAABB.z - 0.005f);
                    // Mark covered
                    ChunkCoord chunkc = ChunkCoord::fromTilePos(tileWorldPos);
                    ChunkID chunkId = chunkc.toGridIDType(mWorld.getWidthChunks());
                    mChunkBuildingData[chunkId].setTileCoveredByBuilding((tileWorldPos.y % CHUNK_WIDTH) * CHUNK_WIDTH + tileWorldPos.x % CHUNK_WIDTH);
                }
                else {
                    // TODO: Guarentee this!
                    break; // Only need to iterate first floor, BP is sorted
                }
            }
        }

        for (int c = 0; c < newBuilding->mChunkDependencyCount; ++c) {
            ChunkID dep = newBuilding->mChunkDependencies[c];
            ChunkBuildingData& buildingData = mChunkBuildingData[dep];
            if (!buildingData.getIsSimulated()) {
                ++newBuilding->mChunkDependenciesActiveCount;
            }

            std::lock_guard lock(buildingData.mMutex);
            buildingData.buildings.emplace_back(newBuilding);
            buildingData.getDisconnectedBuildings().emplace_back(newBuilding);
        }

        if (newBuilding->mChunkDependenciesActiveCount > 0) {
            for (int c = 0; c < newBuilding->mChunkDependencyCount; ++c) {
                ChunkID dep = newBuilding->mChunkDependencies[c];
                ChunkBuildingData& buildingData = mChunkBuildingData[dep];
                ++buildingData.numLoadingBuildingsRef();
            }
            newBuilding->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(tileAABB, newBuilding->mFloorHeight, newBuilding);
            mWorld.getTileContainerLoader().loadBuildingAsync(*newBuilding);
        }
        else {
            newBuilding->mState = BuildingState::SIM;
        }
    });
    return rv;
}

void BuildingGrid::debugRender() {
    constexpr int LIFETIME_FRAMES = 24;
    static int x = 0;
    if (x++ >= LIFETIME_FRAMES) {
        const color4 ACTIVE_COLOR(0, 255, 0, 128);
        const color4 DORMANT_COLOR(255, 255, 0, 128);
        std::shared_lock lock(mBuildingsMutex);
        for (auto&& it : mBuildings) {
            const Building* s = it.second.get();
            switch (s->mState) {
                case BuildingState::ACTIVE:
                    DebugRenderer::drawAABB(s->mTileAABB, ACTIVE_COLOR, LIFETIME_FRAMES);
                    break;
                case BuildingState::SIM:
                    DebugRenderer::drawAABB(s->mTileAABB, DORMANT_COLOR, LIFETIME_FRAMES);
                    break;
                default:
                    assert(false);
            }
        }
    }
}

Building* BuildingGrid::tryGetBuildingAtWorldPos(TileCoord worldPos) const {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorld.getWidthTiles() || worldPos.y >= mWorld.getWidthTiles()) [[unlikely]] {
        return nullptr;
    }
    const DTileCoord dTileCoord(worldPos);
    const ChunkCoord chunkCoord(dTileCoord);
    const DTileCoord dTileOffset = dTileCoord - DTileCoord(chunkCoord);
    const ChunkID chunkId = chunkCoord.toGridIDType(mWorld.getWidthChunks());
    const DTileIndex index = dTileOffset.y * CHUNK_WIDTH_DTILES + dTileOffset.x;

    BuildingID id;
    {
        const ChunkBuildingData& buildingData = mChunkBuildingData[chunkId];
        std::lock_guard innerLock(buildingData.mMutex);
        if (!buildingData.dTileBuildings) {
            return nullptr;
        }
        id = buildingData.dTileBuildings[index];
        if (id == INVALID_BUILDING_ID) {
            return nullptr;
        }
    }
    std::shared_lock lock(mBuildingsMutex);
    auto it = mBuildings.find(id);
    // This can occur if we query while a structure has been placed but not fully allocated and tracked
    if (it == mBuildings.end()) [[unlikely]] {
        return nullptr;
    }
    return it->second.get();
}

ui32 BuildingGrid::allBuildingsLoadedAtChunk(ChunkID chunkId) const {
    ASSERT_GAME_THREAD();
    const ChunkBuildingData& buildingData = mChunkBuildingData[chunkId];
    return buildingData.getNumLoadingBuildings() == 0;
}

void BuildingGrid::connectBuildingsToChunk(Chunk& chunk) {
    ASSERT_GAME_THREAD();
    const ChunkID chunkId = chunk.getChunkID();
    ChunkBuildingData& buildingData = mChunkBuildingData[chunkId];

    std::vector<Building*>& disconnected = buildingData.getDisconnectedBuildings();

    for (int i = disconnected.size() - 1; i >= 0; --i) {
        Building* building = disconnected[i];
        // Can only connect active buildings
        if (building->getState() == BuildingState::ACTIVE) {
            connectBuildingToChunk(*building, chunk);
            disconnected[i] = disconnected.back();
            disconnected.pop_back();
            continue;
        }
    }
}

void BuildingGrid::connectBuildingToChunk(Building& building, Chunk& chunk) {
    ASSERT_GAME_THREAD();
    assert(building.mState == BuildingState::ACTIVE);
    const ChunkID chunkId = chunk.getChunkID();

    // Connect to chunk
    TileContainer& tileContainer = *building.getTileContainer();
    TileContainer* chunkTileContainer = chunk.getTileContainer();
    assert(chunkTileContainer);
    const std::vector<Tile>& tiles = tileContainer.getTiles();
    const TileSpatialGrid& tileSpatialGrid = tileContainer.getTileSpatialGrid();
    const i32v3 dims = tileSpatialGrid.getDims();
    const i32 floorStride = dims.x * dims.y;

    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    const TileSpatialGrid& chunkTileSpatialGrid = chunk.getTileContainer()->getTileSpatialGrid();
    const i32v3& buildingWorldPos = tileSpatialGrid.getWorldPos();
    const i32 floorHeight = tileSpatialGrid.getFloorHeight();
    
    // Clip X and Y to the chunk
    const i32v2 chunkWorldPos = chunk.getWorldPos();
    const i32 xMin = glm::clamp(buildingWorldPos.x, chunkWorldPos.x, chunkWorldPos.x + CHUNK_WIDTH);
    const i32 xMax = glm::clamp(buildingWorldPos.x + dims.x, chunkWorldPos.x, chunkWorldPos.x + CHUNK_WIDTH);
    const i32 yMin = glm::clamp(buildingWorldPos.y, chunkWorldPos.y, chunkWorldPos.y + CHUNK_WIDTH);
    const i32 yMax = glm::clamp(buildingWorldPos.y + dims.y, chunkWorldPos.y, chunkWorldPos.y + CHUNK_WIDTH);
    //const i32v2 xRange = glm::clamp(i32v2(buildingRootPos.x, buildingRootPos.x + dims.x), chunkPos, chunkPos + chunkDims);
    //const i32v2 yRange = glm::clamp(i32v2(buildingRootPos.y, buildingRootPos.y + dims.y), chunkPos, chunkPos + chunkDims);

    constexpr f32 TILE_BLOCK_RANGE = 2.f;

    const i32 localXMin = xMin - buildingWorldPos.x;
    const i32 localXMax = xMax - buildingWorldPos.x;
    const i32 localYMin = yMin - buildingWorldPos.y;
    const i32 localYMax = yMax - buildingWorldPos.y;

    // ChunkGenerator::generateChunkFromSimChunk will set building block and grass block for first floor,
    // based on the ChunkBuildingData::buildingFootprint, so we only need to iterate subsequent floors
    // and check if they are close to intersecting terrain, which can happen with steep hills
    for (i32 z = 1; z < dims.z; ++z) {
        const i32 zOffset = floorStride * z;
        for (i32 y = localYMin; y < localYMax; ++y) {
            const i32 zyOffset = zOffset + y * dims.x;
            const i32 worldPosY = y + buildingWorldPos.y;
            for (i32 x = localXMin; x < localXMax; ++x) {
                const TileIndex buildingTileIndex = zyOffset + x;
                const Tile& tile = tileContainer.getTileAt(buildingTileIndex);
                if (!tile.isEmpty()) {
                    const i32 worldPosX = x + buildingWorldPos.x;
                    TileIndex chunkTileIndex = chunkTileSpatialGrid.getBaseTileIndexFromXYOffset(worldPosX - chunkWorldPos.x, worldPosY - chunkWorldPos.y);
                    // TODO CHECK PROXIMITY!
                    const i32 buildingWorldZ = z * floorHeight + buildingWorldPos.z;
                    // TODO: This should be part of a bulk edit
                    if (buildingWorldZ - chunkTileContainer->getTileAt(chunkTileIndex).getGroundZOffset() <= TILE_BLOCK_RANGE) { 
                        chunkTileContainer->setTileFlag(chunkTileIndex, TileFlags::IS_BLOCKED_BY_BUILDING);
                        chunk.clearGrassAt(chunkTileIndex);
                    }
                }
            }
        }
    }
}

void BuildingGrid::onBuildingFinishedLoad(Building& building) {
    GameThreadTasks::getInstance().addGenericTask([this, &building]() {

        TileContainerEvent loadFinishedEvent;
        loadFinishedEvent.container = building.getTileContainer();
        mWorld.getTileContainerRepository().dispatchLoadFinished(loadFinishedEvent);

        building.mState = BuildingState::ACTIVE;
        for (ui32 i = 0; i < building.getChunkDependencyCount(); ++i) {
            const ChunkID id = building.getChunkDependencies()[i];
            ChunkBuildingData& buildingData = mChunkBuildingData[id];
            assert(buildingData.numLoadingBuildingsRef() > 0);
            --buildingData.numLoadingBuildingsRef();
            Chunk& chunk = mWorld.getChunkGrid().getChunk(id);
            // Check if we are in a state where we should instantly connect
            if (chunk.getState() == ChunkState::ACTIVATED || chunk.getState() == ChunkState::LOADING_MESH_PHYSICS_NAV_VISIBILITY) {
                connectBuildingToChunk(building, chunk);
                // Remove from disconnected array
                std::vector<Building*>& disconnectedBuildings = buildingData.getDisconnectedBuildings();
                bool found = false;
                for (size_t j = 0; j < disconnectedBuildings.size(); ++j) {
                    if (disconnectedBuildings[j] == &building) {
                        disconnectedBuildings[j] = disconnectedBuildings.back();
                        disconnectedBuildings.pop_back();
                        found = true;
                        break;
                    }
                }
                assert(found);
            }
        }

        // Navmesh dirty
        if (NavWorld* navWorld = mWorld.tryGetNavWorld()) {
            navWorld->markContainerNavDirty(building.getTileContainer());
        }
    });
}

void BuildingGrid::initEventHandlers() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);
    chunkGrid.addBeginLoadListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
        ChunkBuildingData& buildingData = mChunkBuildingData[chunk.getChunkID()];
        assert(buildingData.getDisconnectedBuildings().size() == buildingData.buildings.size());
        const std::vector<Building*>& chunkBuildings = buildingData.buildings;

        buildingData.setIsSimulated(false);

        std::lock_guard lock(buildingData.mMutex);
        for (Building* building : chunkBuildings) {
            assert(building->mChunkDependenciesActiveCount < building->getChunkDependencyCount());
            // Activate on the first dependant chunk activation
            if (++building->mChunkDependenciesActiveCount == 1) {
                if (building->mIsDeactivating) {
                    removeBuildingFromDeactivateList(building);
                }
                switch (building->mState.load()) {
                    case BuildingState::LOADING_TILES:
                        // Do nothing, already loading
                        break;
                    case BuildingState::ACTIVE:
                        // Do nothing, we will connect later
                        break;
                    case BuildingState::SIM: {
                        building->mState = BuildingState::WAITING_SIM_RELEASE;
                        building->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(building->getTileAABB(), building->getFloorHeight(), static_cast<Building*>(building));

                        // We must do a handshake to ensure each thread is aware of when it loses control of its data
                        // Incref while we wait for sim release so we dont deactivate during initial handshake for simplicity
                        chunk.incRef();
                        for (int c = 0; c < building->mChunkDependencyCount; ++c) {
                            ChunkID dep = building->mChunkDependencies[c];
                            ChunkBuildingData& buildingData = mChunkBuildingData[dep];
                            ++buildingData.numLoadingBuildingsRef();
                        }
                        mWorld.tryGetHostSimContext()->tryGetSimThread()->addTask([this, &chunk, building]() {
                            // loadBuildingAsync will set state to LOADING_TILES, completing the handshake
                            // All sim thread access up to this point is valid
                            mWorld.getTileContainerLoader().loadBuildingAsync(*building);
                            chunk.decRef();
                        });
                        break;
                    }
                    default:
                        break;

                }
            }
        }
    });

    chunkGrid.addDeactivatedListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        Chunk& chunk = evnt.chunk;
        ASSERT_GAME_THREAD();
        // Move structures to simuation layer
        ChunkBuildingData& buildingData = mChunkBuildingData[chunk.getChunkID()];
        const std::vector<Building*>& buildings = buildingData.buildings;
        std::vector<Building*>& disconnectedBuildings = buildingData.getDisconnectedBuildings();

        buildingData.setIsSimulated(true);
        // Disconnect all

        std::lock_guard lock(buildingData.mMutex);
        // Refill disconnected with all
        disconnectedBuildings.clear();
        disconnectedBuildings.reserve(buildings.size());
        for (Building* building : buildings) {
            // All are now disconnected
            disconnectedBuildings.emplace_back(building);
            assert(building->mChunkDependenciesActiveCount > 0);
            // Deactivate when we have no active chunks
            if (--building->mChunkDependenciesActiveCount == 0) {
                if (!building->mIsDeactivating) {
                    // We cant deactivate a building that is not active or loading
                    assert(building->mState == BuildingState::ACTIVE || building->mState == BuildingState::LOADING_TILES);
                    building->mIsDeactivating = true;
                    // TODO: Once we have async load make sure we handle it properly here
                    mDeactivatingBuildings.emplace_back(building);
                }
            }
        }
    });
}

void BuildingGrid::removeBuildingFromDeactivateList(Building* building) {
    ASSERT_GAME_THREAD();
    for (size_t i = 0; i < mDeactivatingBuildings.size(); ++i) {
        if (mDeactivatingBuildings[i] == building) {
            mDeactivatingBuildings[i] = mDeactivatingBuildings.back();
            mDeactivatingBuildings.pop_back();
            building->mIsDeactivating = false;
            return;
        }
    }
    panic("Didnt find deactivating building");
}
