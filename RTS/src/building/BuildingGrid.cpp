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
    mChunkStructureData = std::make_unique<ChunkBuildingData[]>(SQ(mWorld.getWidthChunks()));
}

void BuildingGrid::tick() {
    ASSERT_GAME_THREAD();
    for (size_t i = 0; i < mDeactivatingStructures.size();) {
        if (mDeactivatingStructures[i]->getRefCount() == 0) {
            mDeactivatingStructures[i]->freeData();
            mDeactivatingStructures[i]->mState = StructureState::SIM;
            mDeactivatingStructures[i] = mDeactivatingStructures.back();
            mDeactivatingStructures.pop_back();
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

        const ChunkBuildingData& structureData = mChunkStructureData[chunkId];
        if (!structureData.dTileStructures) {
            return false;
        }
        const DTileCoord dTileOffset = dTileCoord - DTileCoord(chunkCoord);
        const DTileIndex index = dTileOffset.y * CHUNK_WIDTH_DTILES + dTileOffset.x;
        const BuildingID id = structureData.dTileStructures[index];
        if (id == INVALID_STRUCTURE_ID) {
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
            ChunkBuildingData& structureData = mChunkStructureData[chunkId];
            // Initialize structure data if needed
            if (!structureData.dTileStructures) [[unlikely]] {
                structureData.dTileStructures = std::make_unique<BuildingID[]>(CHUNK_SIZE_DTILES);
                for (ui32 i = 0; i < CHUNK_SIZE_DTILES; ++i) {
                    structureData.dTileStructures[i] = INVALID_STRUCTURE_ID;
                }
            }
            const DTileCoord dTileOffset = worldCoord - DTileCoord(chunkCoord);
            const DTileIndex index = dTileOffset.y * CHUNK_WIDTH_DTILES + dTileOffset.x;
            structureData.dTileStructures[index] = newStructureId;
        }
    }

    assert(ownedDTiles.getNumBits() >= DTileDims.x * DTileDims.y);
    i32v3 tileDims = tileAABB.dims;
    assert((tileDims.z % floorHeight) == 0);
    tileDims.z /= floorHeight;
    assert(tileDims.x < CHUNK_WIDTH&& tileDims.y < CHUNK_WIDTH);
    std::unique_ptr<Building> newStructure = std::make_unique<Building>();
    newStructure->mType = StructureType::Building;
    newStructure->mOwnedDTiles = ownedDTiles;
    newStructure->mTileAABB = tileAABB;
    newStructure->mId = newStructureId;
    newStructure->setBlueprint(std::move(bptr));
   
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
        newStructure->mChunkDependencies[chunkCount++] = c->getChunkID();
    }
    newStructure->mChunkdDependencyCount = chunkCount;
    newStructure->mChunkDependenciesSimulating = 0;
    assert(floorHeight < UINT8_MAX); 
    newStructure->mFloorHeight = floorHeight;

    Building* rv = newStructure.get();
    { // Write critical section
        std::lock_guard lock(mMutex);
        for (int c = 0; c < chunkCount; ++c) {
            ChunkID dep = newStructure->mChunkDependencies[c];
            ChunkBuildingData& structureData = mChunkStructureData[dep];
            structureData.containedStructures.emplace_back(newStructureId);
            if (structureData.isSimulated) {
                newStructure->mState = StructureState::SIM;
                ++newStructure->mChunkDependenciesSimulating;
            }
        }
        if (newStructure->mChunkDependenciesSimulating == 0) {
            newStructure->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(tileAABB, floorHeight, (Building*)newStructure.get());
            mWorld.getTileContainerLoader().loadBuilding(static_cast<Building&>(*newStructure));
            rv->mState = StructureState::ACTIVE;
        }
        else {
            rv->mState = StructureState::SIM;
        }
        mStructures[rv->mId] = std::move(newStructure);
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
        for (auto&& it : mStructures) {
            const Building* s = it.second.get();
            switch (s->mState) {
                case StructureState::ACTIVE:
                    DebugRenderer::drawAABB(s->mTileAABB, ACTIVE_COLOR, LIFETIME_FRAMES);
                    break;
                case StructureState::SIM:
                    DebugRenderer::drawAABB(s->mTileAABB, DORMANT_COLOR, LIFETIME_FRAMES);
                    break;
                default:
                    assert(false);
            }
        }
    }
}

Building* BuildingGrid::tryGetStructureAtWorldPos(TileCoord worldPos) const {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorld.getWidthTiles() || worldPos.y >= mWorld.getWidthTiles()) [[unlikely]] {
        return nullptr;
    }
    const DTileCoord dTileCoord(worldPos);
    const ChunkCoord chunkCoord(dTileCoord);
    const DTileCoord dTileOffset = dTileCoord - DTileCoord(chunkCoord);
    const ChunkID chunkId = chunkCoord.toGridIDType(mWorld.getWidthChunks());
    const DTileIndex index = dTileOffset.y * CHUNK_WIDTH_DTILES + dTileOffset.x;

    std::shared_lock lock(mMutex);
    const ChunkBuildingData& structureData = mChunkStructureData[chunkId];
    if (!structureData.dTileStructures) {
        return nullptr;
    }
    const BuildingID id = structureData.dTileStructures[index];
    if (id == INVALID_STRUCTURE_ID) {
        return nullptr;
    }
    auto it = mStructures.find(id);
    // This can occur if we query while a structure has been placed but not fully allocated and tracked
    if (it == mStructures.end()) [[unlikely]] {
        return nullptr;
    }
    return it->second.get();
}


void BuildingGrid::initEventHandlers() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);
    chunkGrid.addReadyListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
        ChunkBuildingData& structureData = mChunkStructureData[chunk.getChunkID()];
        const std::vector<BuildingID>& chunkStructures = structureData.containedStructures;

        std::lock_guard lock(mMutex);
        structureData.isSimulated = false;
        for (BuildingID structureID : chunkStructures) {
            auto&& it = mStructures.find(structureID);
            assert(it != mStructures.end());
            Building* structure = it->second.get();
            if (--structure->mChunkDependenciesSimulating == 0) {
                if (structure->mState == StructureState::DEACTIVATING) {
                    removeStructureFromDeactivateList(structure);
                }
                structure->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(structure->getTileAABB(), structure->getFloorHeight(), static_cast<Building*>(structure));
                assert(structure->getType() == StructureType::Building);
                mWorld.getTileContainerLoader().loadBuilding(static_cast<Building&>(*structure));
                structure->mState = StructureState::ACTIVE;
            }
        }
    });

    chunkGrid.addDeactivateListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        Chunk& chunk = evnt.chunk;
        ASSERT_GAME_THREAD();
        // Move structures to simuation layer
        ChunkBuildingData& structureData = mChunkStructureData[chunk.getChunkID()];
        const std::vector<BuildingID>& chunkStructures = structureData.containedStructures;

        std::lock_guard lock(mMutex);
        structureData.isSimulated = true;
        for (BuildingID structureID : chunkStructures) {
            auto&& it = mStructures.find(structureID);
            assert(it != mStructures.end());
            Building* structure = it->second.get();
            ++structure->mChunkDependenciesSimulating;
            if (structure->mState != StructureState::DEACTIVATING) {
                structure->mState = StructureState::DEACTIVATING;
                mDeactivatingStructures.emplace_back(structure);
            }
        }
    });
}

void BuildingGrid::removeStructureFromDeactivateList(Building* structure) {
    for (size_t i = 0; i < mDeactivatingStructures.size(); ++i) {
        if (mDeactivatingStructures[i] == structure) {
            mDeactivatingStructures[i] = mDeactivatingStructures.back();
            mDeactivatingStructures.pop_back();
            return;
        }
    }
    assert(false);
}
