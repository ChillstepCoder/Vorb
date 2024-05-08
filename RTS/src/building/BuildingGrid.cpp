#include "stdafx.h"

#include "building/building.h"
#include "BuildingGrid.h"

#include "tile/TileContainerRepository.h"

#include "world/World.h"
#include "tile/TileContainerLoader.h"

#include "debugging/DebugRenderer.h"

#include "boost/container/flat_set.hpp"

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
            bldg->freeData();
            bldg->mState = BuildingState::SIM;
            for (ui32 c = 0; c < bldg->mChunkDependencyCount; ++c) {
                ChunkID id = bldg->mChunkDependencies[c];
                ChunkBuildingData& buildingData = mChunkBuildingData[id];
                assert(buildingData.numSimulatedBuildings < bldg->mChunkDependencyCount);
                ++buildingData.numSimulatedBuildings;
            }
            bldg = mDeactivatingBuildings.back();
            mDeactivatingBuildings.pop_back();
        } else {
            ++i;
        }
    }
}

Building* BuildingGrid::tryMakeNewBuilding(const i32AABB3& tileAABB, ui32 floorHeight, const BitArray& ownedDTiles, std::unique_ptr<BuildingBlueprint>& bptr) {
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
        std::lock_guard lock(mMutex);
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
    assert(floorHeight < UINT8_MAX); 
    newBuilding->mFloorHeight = floorHeight;

    Building* rv = newBuilding.get();
    { // Write critical section
        std::lock_guard lock(mMutex);
        for (int c = 0; c < chunkCount; ++c) {
            ChunkID dep = newBuilding->mChunkDependencies[c];
            ChunkBuildingData& buildingData = mChunkBuildingData[dep];
            buildingData.containedBuildings.emplace_back(newStructureId);
            // Building always initializes as "simulating", this is decremented in onBuildingFinishedLoad
            ++buildingData.numSimulatedBuildings;
            if (!buildingData.isSimulated) {
                ++newBuilding->mChunkDependenciesActive;
            }
        }
        if (newBuilding->mChunkDependenciesActive > 0) {
            newBuilding->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(tileAABB, floorHeight, (Building*)newBuilding.get());
            mWorld.getTileContainerLoader().loadBuilding(static_cast<Building&>(*newBuilding));
            onBuildingFinishedLoad(*rv);
        }
        else {
            rv->mState = BuildingState::SIM;
        }
        mBuildings[rv->mId] = std::move(newBuilding);
    }
    return rv;
}

void BuildingGrid::debugRender() {
    constexpr int LIFETIME_FRAMES = 24;
    static int x = 0;
    if (x++ >= LIFETIME_FRAMES) {
        const color4 ACTIVE_COLOR(0, 255, 0, 128);
        const color4 DORMANT_COLOR(255, 255, 0, 128);
        std::lock_guard lock(mMutex);
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

    std::shared_lock lock(mMutex);
    const ChunkBuildingData& buildingData = mChunkBuildingData[chunkId];
    if (!buildingData.dTileBuildings) {
        return nullptr;
    }
    const BuildingID id = buildingData.dTileBuildings[index];
    if (id == INVALID_BUILDING_ID) {
        return nullptr;
    }
    auto it = mBuildings.find(id);
    // This can occur if we query while a structure has been placed but not fully allocated and tracked
    if (it == mBuildings.end()) [[unlikely]] {
        return nullptr;
    }
    return it->second.get();
}

void BuildingGrid::onBuildingFinishedLoad(Building& building) {
    ASSERT_GAME_THREAD();
    building.mState = BuildingState::ACTIVE;
    for (ui32 i = 0; i < building.getChunkDependencyCount(); ++i) {
        const ChunkID id = building.getChunkDependencies()[i];
        ChunkBuildingData& buildingData = mChunkBuildingData[id];
        assert(buildingData.numSimulatedBuildings > 0);
        --buildingData.numSimulatedBuildings;
    }
}

void BuildingGrid::initEventHandlers() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);
    chunkGrid.addBeginLoadListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
        ChunkBuildingData& buildingData = mChunkBuildingData[chunk.getChunkID()];
        const std::vector<BuildingID>& chunkBuildings = buildingData.containedBuildings;

        std::lock_guard lock(mMutex);
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
                else {
                    building->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(building->getTileAABB(), building->getFloorHeight(), static_cast<Building*>(building));
                    // TODO: Do this async on worker thread!
                    mWorld.getTileContainerLoader().loadBuilding(*building);
                    onBuildingFinishedLoad(*building);
                }
            }
        }
    });

    chunkGrid.addDeactivatedListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        Chunk& chunk = evnt.chunk;
        ASSERT_GAME_THREAD();
        // Move structures to simuation layer
        ChunkBuildingData& structureData = mChunkBuildingData[chunk.getChunkID()];
        const std::vector<BuildingID>& chunkStructures = structureData.containedBuildings;

        std::lock_guard lock(mMutex);
        structureData.isSimulated = true;
        for (BuildingID structureID : chunkStructures) {
            auto&& it = mBuildings.find(structureID);
            assert(it != mBuildings.end());
            Building* building = it->second.get();
            assert(building->mChunkDependenciesActive > 0);
            // Deactivate when we have no active chunks
            if (--building->mChunkDependenciesActive == 0) {
                if (building->mState != BuildingState::DEACTIVATING) {
                    // We cant deactivate a building that is not active
                    assert(building->mState == BuildingState::ACTIVE);
                    building->mState = BuildingState::DEACTIVATING;
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
            building->mState = BuildingState::ACTIVE;
            return;
        }
    }
    panic("Didnt find deactivating building");
}
