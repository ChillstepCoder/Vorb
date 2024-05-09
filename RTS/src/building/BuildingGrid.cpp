#include "stdafx.h"

#include "building/Building.h"
#include "building/BuildingBlueprint.h"
#include "BuildingGrid.h"

#include "gamethread/GameThreadTasks.h"

#include "tile/TileContainerRepository.h"

#include "world/World.h"
#include "tile/TileContainerLoader.h"

#include "debugging/DebugRenderer.h"

#include "boost/container/flat_set.hpp"

// HOW IT WORKS
// 1. Sim thread OR game thread create a building, gets marked as SIM state and disconnected
// 2. Building gets sent to game thread to load
// 3. Once 

// TODO: Serialize this?
BuildingID sStructureIdGen = 0;

BuildingGrid::BuildingGrid(World& world) : mWorld(world) {
    initEventHandlers();
    mChunkBuildingData = std::make_unique<ChunkBuildingData[]>(SQ(mWorld.getWidthChunks()));
}

void BuildingGrid::tick() {
    ASSERT_GAME_THREAD();
    for (size_t i = 0; i < mDeactivatingBuildings.size();) {
        Building* bldg = mDeactivatingBuildings[i];
        if (bldg->getRefCount() == 0) {
            bldg->mState = BuildingState::SIM;
            bldg->freeData();
            for (ui32 c = 0; c < bldg->mChunkDependencyCount; ++c) {
                ChunkID id = bldg->mChunkDependencies[c];
                ChunkBuildingData& buildingData = mChunkBuildingData[id];
                buildingData.simulatedBuildings.emplace_back(bldg->getId());
            }
            bldg = mDeactivatingBuildings.back();
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

    BuildingID newStructureId;

    // First check if we can actually place structure here, and place them in one lock
    // Must be done this way to avoid any race conditions
    { // Critical section
        std::lock_guard lock(mBuildingsMutex);
        newStructureId = sStructureIdGen;
        // Check
        for (ui32 di = 0; di < coveredTilesCount; ++di) {
            const DTileCoord worldCoord = coveredTiles[di];
            if (structureExists(worldCoord)) {
                return nullptr;
            }
        }

        // Allocate
        newStructureId = sStructureIdGen++;

        // Place
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
            structureData.dTileBuildings[index] = newStructureId;
        }
    }

    assert(ownedDTiles.getNumBits() >= DTileDims.x * DTileDims.y);
    i32v3 tileDims = tileAABB.dims;
    assert((tileDims.z % floorHeight) == 0);
    tileDims.z /= floorHeight;
    assert(tileDims.x < CHUNK_WIDTH&& tileDims.y < CHUNK_WIDTH);
    std::unique_ptr<Building> newBuilding = std::make_unique<Building>();
    newBuilding->mOwnedDTiles = ownedDTiles;
    newBuilding->mTileAABB = tileAABB;
    newBuilding->mId = newStructureId;
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
    newBuilding->mChunkDependenciesActive = 0;
    newBuilding->mChunkDependenciesConnected = 0;
    assert(floorHeight < UINT8_MAX); 
    newBuilding->mFloorHeight = floorHeight;

    // Flatten terrain and create building
    GameThreadTasks::getInstance().addGenericTask([this, newBuilding = newBuilding.get(), tileAABB]() {
        IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
        BuildingBlueprint& bp = *newBuilding->getBlueprint();
        const i32v2 worldPos = bp.worldPosRootDTile.toTilePos();
        const i32 floorStride = tileAABB.dims.x * tileAABB.dims.y;
        for (ui32 i = 0; i < bp.tileTargetCount; ++i) {
            BuildingBlueprintTileTarget& tileTarget = bp.tileTargets[i];
            if (tileTarget.tileIndex < floorStride) {
                i32v2 tileWorldPos = worldPos + i32v2(tileTarget.tileIndex % tileAABB.dims.x, tileTarget.tileIndex / tileAABB.dims.x);
                // Epsilon to prevent z fighting
                heightGrid.setHeightAtWorldPos(tileWorldPos, tileAABB.z - 0.005f);
            }
        }

        { // Write critical section
            for (int c = 0; c < newBuilding->mChunkDependencyCount; ++c) {
                ChunkID dep = newBuilding->mChunkDependencies[c];
                ChunkBuildingData& buildingData = mChunkBuildingData[dep];
                std::lock_guard lock(buildingData.mMutex);
                if (!buildingData.isSimulated) {
                    ++newBuilding->mChunkDependenciesActive;
                }
            }
            if (newBuilding->mChunkDependenciesActive > 0) {
                newBuilding->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(tileAABB, newBuilding->mFloorHeight, newBuilding);

                newBuilding->mState = BuildingState::LOADING;
                mWorld.getTileContainerLoader().loadBuilding(*newBuilding);
                onBuildingFinishedLoad(*newBuilding);
            }
            else {
                newBuilding->mState = BuildingState::SIM;
            }
        }
    });
    Building* rv = newBuilding.get();
    {
        std::lock_guard lock(mBuildingsMutex);
        mBuildings[newBuilding->mId] = std::move(newBuilding);
    }
    return rv;
}

void BuildingGrid::debugRender() {
    constexpr int LIFETIME_FRAMES = 24;
    static int x = 0;
    if (x++ >= LIFETIME_FRAMES) {
        const color4 ACTIVE_COLOR(0, 255, 0, 128);
        const color4 DORMANT_COLOR(255, 255, 0, 128);
        std::lock_guard lock(mBuildingsMutex);
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

void BuildingGrid::onBuildingFinishedLoad(Building& building) {
    ASSERT_GAME_THREAD();
    for (ui32 i = 0; i < building.getChunkDependencyCount(); ++i) {
        const ChunkID id = building.getChunkDependencies()[i];
        ChunkBuildingData& buildingData = mChunkBuildingData[id];
        std::lock_guard lock(buildingData.mMutex);
        assert(buildingData.mNumLoadingBuildings > 0);
        --buildingData.mNumLoadingBuildings;
    }
    building.mState = BuildingState::ACTIVE;
}

void BuildingGrid::initEventHandlers() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);
    chunkGrid.addBeginLoadListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
        ChunkBuildingData& buildingData = mChunkBuildingData[chunk.getChunkID()];
        assert(buildingData.connectedBuildings.size() == 0);
        const std::vector<BuildingID>& chunkBuildings = buildingData.disconnectedBuildings;

        std::lock_guard lock(buildingData.mMutex);
        buildingData.isSimulated = false;
        for (BuildingID buildingId : chunkBuildings) {
            auto&& it = mBuildings.find(buildingId);
            assert(it != mBuildings.end());
            Building* building = it->second.get();
            assert(building->mChunkDependenciesActive < building->getChunkDependencyCount());
            // Activate on the first dependant chunk activation
            if (++building->mChunkDependenciesActive == 1) {
                if (building->mState == BuildingState::DEACTIVATING) [[unlikely]] {
                    // No need to re-load as we already were loaded
                    removeBuildingFromDeactivateList(building);
                }
                else if (building->mState == BuildingState::SIM) {
                    building->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(building->getTileAABB(), building->getFloorHeight(), static_cast<Building*>(building));
                    // TODO: Do this async on worker thread!
                    building->mState = BuildingState::LOADING;
                    mWorld.getTileContainerLoader().loadBuilding(*building);
                    onBuildingFinishedLoad(*building);
                }
                // If we arent sim or deactivating, we should be loading, so do nothing as we will load as part of that process
            }
        }
    });

    chunkGrid.addDeactivatedListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        Chunk& chunk = evnt.chunk;
        ASSERT_GAME_THREAD();
        // Move structures to simuation layer
        ChunkBuildingData& structureData = mChunkBuildingData[chunk.getChunkID()];
        std::vector<BuildingID>& connectedBuildings = structureData.connectedBuildings;
        std::vector<BuildingID>& disconnectedBuildings = structureData.disconnectedBuildings;

        std::lock_guard lock(structureData.mMutex);
        structureData.isSimulated = true;
        for (BuildingID structureID : connectedBuildings) {
            auto&& it = mBuildings.find(structureID);
            assert(it != mBuildings.end());
            Building* building = it->second.get();
            assert(building->mChunkDependenciesActive > 0);
            // Deactivate when we have no active chunks
            if (--building->mChunkDependenciesActive == 0) {
                if (building->mState != BuildingState::DEACTIVATING) {
                    // We cant deactivate a building that is not active
                    assert(building->mState == BuildingState::ACTIVE);
                    assert(building->mState != BuildingState::LOADING && "NEED TO HANDLE LOAD ON DEACTIVATE"); x; // TODO
                    building->mState = BuildingState::DEACTIVATING;
                    // TODO: Once we have async load make sure we handle it properly here
                    mDeactivatingBuildings.emplace_back(building);
                }
            }
        }
        disconnectedBuildings.reserve(disconnectedBuildings.size() + connectedBuildings.size());
        disconnectedBuildings.insert(disconnectedBuildings.begin(), connectedBuildings.begin(), connectedBuildings.end());
    });
}

void BuildingGrid::removeBuildingFromDeactivateList(Building* building) {
    ASSERT_GAME_THREAD();
    for (size_t i = 0; i < mDeactivatingBuildings.size(); ++i) {
        if (mDeactivatingBuildings[i] == building) {
            mDeactivatingBuildings[i] = mDeactivatingBuildings.back();
            mDeactivatingBuildings.pop_back();
            building->mState = BuildingState::ACTIVE;
            return;
        }
    }
    panic("Didnt find deactivating building");
}
