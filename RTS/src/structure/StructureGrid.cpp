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
}

Structure* StructureGrid::makeNewStructure(StructureType type, const i32AABB3& aabb, ui32 floorHeight, const BitArray& ownedDTiles) {
    ASSERT_GAME_THREAD();
    assert(ownedDTiles.getNumBits() >= (aabb.dims.x >> 1) * (aabb.dims.y >> 1));
    i32v3 tileDims = aabb.dims;
    assert((tileDims.z % floorHeight) == 0);
    tileDims.z /= floorHeight;
    assert(tileDims.x < CHUNK_WIDTH&& tileDims.y < CHUNK_WIDTH);
    std::unique_ptr<Structure> newStructure;
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    switch (type) {
        case StructureType::Building: {
            newStructure = std::make_unique<Building>();
            newStructure->mType = StructureType::Building;
            newStructure->mTileContainer = mWorld.getTileContainerRepository().createNewEmptyBuildingContainer(aabb.pos, tileDims, floorHeight, (Building*)newStructure.get());
            newStructure->mOwnedDTiles = ownedDTiles;
            break;
        }
        default:
            assert(false && "Invalid structure type");
    }
    newStructure->mTileAABB = aabb;
    newStructure->mId = sStructureIdGen++;

    const StructureBBox newBox(StructureBoxPoint(aabb.x, aabb.y), StructureBoxPoint(aabb.x + aabb.width, aabb.y + aabb.depth));
    mSpatialLookup.insert(StructureRegion{ newBox, newStructure->mId });

    Structure* rv = newStructure.get();
    {
        std::lock_guard lock(mMutex);
        mStructures[newStructure->mId] = std::move(newStructure);
    }

    // Hook up chunk dependencies
    i32v2 worldXY;
    boost::container::flat_set<Chunk*> chunkDependencies;
    chunkDependencies.reserve(4);
    worldXY = i32v2(aabb.x, aabb.y);
    chunkDependencies.insert(&chunkGrid.getChunkAtPosition(worldXY));
    worldXY = i32v2(aabb.x + aabb.dims.x, aabb.y);
    chunkDependencies.insert(&chunkGrid.getChunkAtPosition(worldXY));
    worldXY = i32v2(aabb.x, aabb.y + aabb.dims.y);
    chunkDependencies.insert(&chunkGrid.getChunkAtPosition(worldXY));
    worldXY = i32v2(aabb.x + aabb.dims.x, aabb.y + aabb.dims.y);
    chunkDependencies.insert(&chunkGrid.getChunkAtPosition(worldXY));

    assert(chunkDependencies.size() && chunkDependencies.size() <= 4);
    int chunkCount = 0;
    for (auto&& c : chunkDependencies) {
        assert(c->isDataReady());
        // Chunks increment our refcount while they are loaded
        rv->mChunkDependencies[chunkCount++] = c->getChunkID();
        c->addStructure(rv);
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

std::vector<Structure*> StructureGrid::tryGetStructuresAtWorldPos(const i32v2& worldPos) const {
    ASSERT_GAME_THREAD();
    std::vector<StructureRegion> overlappingStructures;
    overlappingStructures.reserve(4);
    // https://valelab4.ucsf.edu/svn/3rdpartypublic/boost-versions/boost_1_55_0/libs/geometry/doc/html/geometry/spatial_indexes/queries.html
    const size_t overlapCount = mSpatialLookup.query(boost::geometry::index::intersects(StructureBoxPoint(worldPos.x, worldPos.y)), std::back_inserter(overlappingStructures));

    std::vector<Structure*> rv;
    rv.reserve(overlappingStructures.size());
    for (auto& it : overlappingStructures) {
        auto&& it2 = mStructures.find(it.id);
        assert(it2 != mStructures.end());
        rv.emplace_back(it2->second.get());
    }
    return rv;
}

void StructureGrid::initEventHandlers() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkEventListeners);
    chunkGrid.addReadyListener(mChunkEventListeners, [this](Chunk& chunk) {
        ASSERT_GAME_THREAD();
        auto&& it = mDormantStructures.find(chunk.getChunkID());
        if (it == mDormantStructures.end()) {
            return;
        }
        std::vector<StructureID>& chunkDormantList = it->second;
        for (StructureID structureID : chunkDormantList) {
            auto&& it = mStructures.find(structureID);
            assert(it != mStructures.end());
            Structure* structure = it->second.get();
            assert(structure->mChunkDependenciesUnloaded > 0);
            if (--structure->mChunkDependenciesUnloaded == 0) {
                assert(structure->mState != StructureState::ACTIVE);
                structure->mState = StructureState::ACTIVE;
                // TODO: OnActive?
            }
            // Calls incref
            chunk.addStructure(structure);
        }
        mDormantStructures.erase(it);
    });

    chunkGrid.addDestroyListener(mChunkEventListeners, [this](Chunk& chunk) {
        ASSERT_GAME_THREAD();
        // Move structures to dormancy
        const std::vector<StructureID>& chunkStructures = chunk.getStructures();
        if (chunk.getStructures().empty()) {
            return;
        }
        std::vector<StructureID>& chunkDormantList = mDormantStructures[chunk.getChunkID()];
        chunkDormantList.reserve(chunkDormantList.size() + chunkStructures.size());
        for (StructureID structureID : chunkStructures) {
            auto&& it = mStructures.find(structureID);
            assert(it != mStructures.end());
            Structure* structure = it->second.get();
            structure->decRef(); // Chunk no longer needs
            ++structure->mChunkDependenciesUnloaded;
            if (structure->mState != StructureState::SIM) {
                structure->mState = StructureState::SIM;
                // TODO: OnDormant?
            }
            chunkDormantList.emplace_back(structureID);
        }
    });
}
