#include "stdafx.h"

#include "city/Building.h"
#include "StructureGrid.h"

#include "tile/TileContainerRepository.h"

#include "world/World.h"

#include "debugging/DebugRenderer.h"'

#include "boost/container/flat_set.hpp"

// STRUCTURE LOADING
// When a chunk that has a structure is loaded, it notifies structure manager to load the structure.
// Structure knows which chunks need to be loaded, ALL chunks must be active for structure to be active.
// Structure can be in two states
// 1. ACTIVE: All chunks are loaded - Tile container exists, navgraph + physics + fine mesh are generated
// 2. DORMANT: Not all chunks are loaded - Mesh LOD can exist (Serialized to disk so we don't need to load tiles? How do we handle buildings in construction?)

// * When an active structure becomes dormant, we deallocate its tiles
// * When a dormant structure becomes active, we allocate its tiles and do all the rest

// TODO: Serialize this
StructureID sStructureIdGen = 0;

StructureGrid::StructureGrid(World& world) : mWorld(world) {
    initEventHandlers();
    mChunkStructureData = std::make_unique<ChunkStructureData[]>(SQ(mWorld.getWidthChunks()));
}

Structure* StructureGrid::tryMakeNewStructure(StructureType type, const i32AABB3& tileAABB, ui32 floorHeight, const BitArray& ownedDTiles) {
    const DTileCoord rootDTileCoord = DTileCoord::fromTilePosRound(tileAABB.pos);
    const ui32v2 DTileDims((tileAABB.dims.x >> 1), (tileAABB.dims.y >> 1));
    // TODO: Cache iter positions without lock?
    // First check if we can actually place structure here, and if so, place it with lock
    {
        DTileCoord iter = rootDTileCoord;
        std::lock_guard lock(mMutex);
        for (iter.y = rootDTileCoord.y; iter.y < rootDTileCoord.y + DTileDims.y; ++iter.y) {
            for (iter.x = rootDTileCoord.x; iter.x < rootDTileCoord.x + DTileDims.x; ++iter.x) {
                x;
            }
        }
    }

    assert(ownedDTiles.getNumBits() >= DTileDims.x * DTileDims.y);
    i32v3 tileDims = tileAABB.dims;
    assert((tileDims.z % floorHeight) == 0);
    tileDims.z /= floorHeight;
    assert(tileDims.x < CHUNK_WIDTH&& tileDims.y < CHUNK_WIDTH);
    std::unique_ptr<Structure> newStructure;
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    switch (type) {
        case StructureType::Building: {
            newStructure = std::make_unique<Building>();
            newStructure->mType = StructureType::Building;
            newStructure->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(tileAABB.pos, tileDims, floorHeight, (Building*)newStructure.get());
            newStructure->mOwnedDTiles = ownedDTiles;
            break;
        }
        default:
            assert(false && "Invalid structure type");
    }
    newStructure->mTileAABB = tileAABB;
    newStructure->mId = sStructureIdGen++;

    Structure* rv = newStructure.get();
    {
        std::lock_guard lock(mMutex);
        mStructures[newStructure->mId] = std::move(newStructure);
    }

    // Hook up chunk dependencies
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
        assert(c->isDataReady());
        // Chunks increment our refcount while they are loaded
        rv->mChunkDependencies[chunkCount++] = c->getChunkID();
    }
    while (chunkCount < 4) {
        rv->mChunkDependencies[chunkCount++] = INVALID_CHUNK_ID;
    }

    rv->mState = StructureState::ACTIVE;
    return rv;
}

void StructureGrid::debugRender() {
    constexpr int LIFETIME_FRAMES = 24;
    static int x = 0;
    if (x++ >= LIFETIME_FRAMES) {
        const color4 ACTIVE_COLOR(0, 255, 0, 128);
        const color4 DORMANT_COLOR(255, 255, 0, 128);
        std::lock_guard lock(mMutex);
        for (auto&& it : mStructures) {
            const Structure* s = it.second.get();
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

Structure* StructureGrid::tryGetStructureAtWorldPos(TileCoord worldPos) const {
    if (worldPos.x < 0 || worldPos.y < 0 || worldPos.x >= mWorld.getWidthTiles() || worldPos.y >= mWorld.getWidthTiles()) [[unlikely]] {
        return nullptr;
    }
    const DTileCoord dTileCoord(worldPos);
    const ChunkCoord chunkCoord(dTileCoord);
    const DTileCoord dTileOffset = dTileCoord - DTileCoord(chunkCoord);
    const ChunkID chunkId = chunkCoord.toGridIDType(mWorld.getWidthChunks());
    const DTileIndex index = dTileOffset.y * CHUNK_WIDTH_DTILES + dTileOffset.x;

    std::shared_lock lock(mMutex);
    const ChunkStructureData& structureData = mChunkStructureData[chunkId];
    if (!structureData.dTileStructures) {
        return nullptr;
    }
    const StructureID id = structureData.dTileStructures[index];
    if (id == INVALID_STRUCTURE_ID) {
        return nullptr;
    }
    auto it = mStructures.find(id);
    assert(it != mStructures.end());
    return it->second.get();
}

void StructureGrid::initEventHandlers() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);
    chunkGrid.addReadyListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        Chunk& chunk = evnt.chunk;
        const ChunkStructureData& structureData = mChunkStructureData[chunk.getChunkID()];
        const std::vector<StructureID>& chunkStructures = structureData.containedStructures;

        std::lock_guard lock(mMutex);

        for (StructureID structureID : chunkStructures) {
            auto&& it = mStructures.find(structureID);
            assert(it != mStructures.end());
            Structure* structure = it->second.get();
            structure->incRef(); // Chunk no longer needs
            if (--structure->mChunkDependenciesUnloaded == 0) {
                // TODO: Load the structure on threadpool
                structure->mState = StructureState::ACTIVE;
            }
        }
    });

    chunkGrid.addDeactivateListener(mChunkEventListeners, [this](ChunkGridEvent& evnt) {
        Chunk& chunk = evnt.chunk;
        ASSERT_GAME_THREAD();
        // Move structures to dormancy
        const ChunkStructureData& structureData = mChunkStructureData[chunk.getChunkID()];
        const std::vector<StructureID>& chunkStructures = structureData.containedStructures;

        std::lock_guard lock(mMutex);

        for (StructureID structureID : chunkStructures) {
            auto&& it = mStructures.find(structureID);
            assert(it != mStructures.end());
            Structure* structure = it->second.get();
            structure->decRef(); // Chunk no longer needs
            ++structure->mChunkDependenciesUnloaded;
            if (structure->mState != StructureState::SIM) {
                structure->mState = StructureState::SIM;
            }
        }
    });
}
