#include "stdafx.h"
#include "GrassMeshManager.h"

#include "rendering/ChunkGrassQuadtree.h"

#include "world/IChunkGrid.h"
#include "world/World.h"
#include "world/Chunk.h"

#include "options/DebugOptions.h"

GrassMeshManager::GrassMeshManager(World& world) : mWorld(world) {
    // TODO: LISTENERS!
    LOG_CRITICAL("Missing event listeners in GrassMeshManager::GrassMeshManager");
    world.getChunkGrid().addReadyListener([this](const Chunk& chunk) {
        mChunkTrackChanges.enqueue(std::make_pair(chunk.getChunkID(), true));
    });
    world.getChunkGrid().addDestroyListener([this](const Chunk& chunk) {
        mChunkTrackChanges.enqueue(std::make_pair(chunk.getChunkID(), false));
    });
}

GrassMeshManager::~GrassMeshManager() {

}

void GrassMeshManager::frameUpdate(const f32v2& loadCenter, f32 elapsedSec) {
    ASSERT_RENDER_THREAD();
    PROFILE_FUNCTION();

    constexpr size_t BULK_SIZE = 64;
    std::pair<ChunkID, bool /*startTracking*/> trackingChanges[BULK_SIZE];
    if (size_t count = mChunkTrackChanges.try_dequeue_bulk(trackingChanges, BULK_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            auto&& data = trackingChanges[i];
            if (data.second) {
                trackChunk(data.first);
            }
            else {
                stopTrackingChunk(data.first);
            }
        }
    }

    for (TrackedChunk& trackedChunk : mTrackedChunks) {
        const f32 distSq = glm::length2(trackedChunk.worldPosCenter - loadCenter);
        std::unique_ptr<ChunkGrassQuadtree>& grassQuadtree = trackedChunk.quadtree;
        if (grassQuadtree) {
            grassQuadtree->update(loadCenter, elapsedSec);
            if (distSq > sDebugOptions.mGrassSettings.distanceSq + 10.0f) {
                if (grassQuadtree->getRefCount() == 0) {
                    TrackedChunkLookupData* lookupData;
                    { // Critical section
                        std::lock_guard lock(mTrackedChunksMutex);
                        lookupData = &mTrackedChunksLookup[trackedChunk.chunk->getChunkID()];
                        assert(lookupData->isActive);
                        lookupData->isActive = false;
                    }

                    // Unregister for events
                    Chunk& chunkNonConst = const_cast<Chunk&>(*trackedChunk.chunk);
                    TileContainer* container = chunkNonConst.getTileContainer();
                    container->removeEditTilesListener(lookupData->eventHandle.first);
                    chunkNonConst.removeGrassEditListener(lookupData->eventHandle.second);

                    grassQuadtree.reset();
                    trackedChunk.chunk->decRef();
                }
            }
            else {
                grassQuadtree->update(loadCenter, elapsedSec);
            }
        }
        else if (distSq < sDebugOptions.mGrassSettings.distanceSq) {
            if (trackedChunk.chunk->tryAquireThreadSafe()) {
                TrackedChunkLookupData* lookupData;
                { // Critical section
                    std::lock_guard lock(mTrackedChunksMutex);
                    lookupData = &mTrackedChunksLookup[trackedChunk.chunk->getChunkID()];
                    assert(!lookupData->isActive);
                    lookupData->isActive = true;
                }
                // Register for edit events  // Const cast ~ get fucked
                Chunk& chunkNonConst = const_cast<Chunk&>(*trackedChunk.chunk);
                TileContainer* container = chunkNonConst.getTileContainer();
                lookupData->eventHandle = std::make_pair(
                    container->addEditTilesListener([this](const TileContainerEvent& evnt) {
                    PROFILE_SCOPE("GrassEdit Dirty");
                    ASSERT_GAME_THREAD();

                    const TileContainerEditEvent& editEvent = std::get<TileContainerEditEvent>(evnt.varEvent);

                    if (editEvent.type == TileContainerEditEventType::ChangeZPos) {
                        // TODO: We could build a bulk enqueue array
                        std::lock_guard lock(mTrackedChunksMutex);
                        auto&& it = mTrackedChunksLookup.find(evnt.container->getOwnerChunk()->getChunkID());
                        assert(it != mTrackedChunksLookup.end() && it->second.isActive);
                        for (ui32 i = 0; i < editEvent.editCount; ++i) {
                            TileContainerEditZPosEventData& data = editEvent.changeZPosArray[i];
                            mTileContainerEdits.enqueue(data.worldPosition);
                        }
                    }
                }),
                    chunkNonConst.addGrassEditListener([this](const ChunkEvent& evnt) {
                    std::lock_guard lock(mTrackedChunksMutex);
                    auto&& it = mTrackedChunksLookup.find(evnt.chunk.getChunkID());
                    assert(it != mTrackedChunksLookup.end() && it->second.isActive);
                    const f32v2 worldPos = evnt.chunk.getTileContainer()->getTileSpatialGrid().getTileBaseWorldPos2D(evnt.tileIndex);
                    mTileContainerEdits.enqueue(worldPos);
                }));
                grassQuadtree = std::make_unique<ChunkGrassQuadtree>(*trackedChunk.chunk);
            }
        }
    }
}

void GrassMeshManager::trackChunk(ChunkID chunkId) {
    const Chunk& chunk = mWorld.getChunkGrid().getChunk(chunkId);
    std::lock_guard lock(mTrackedChunksMutex);
    auto&& it = mTrackedChunksLookup.find(chunkId);
    if (it == mTrackedChunksLookup.end()) {

        mTrackedChunksLookup.emplace(chunkId, TrackedChunkLookupData{
            .index = (ui32)mTrackedChunks.size(),
            .isActive = false
            });
        mTrackedChunks.emplace_back(TrackedChunk{ nullptr, &chunk, chunk.getWorldPosCenter2D() });
        // Destroy will be handled by removeGrassForChunk
    }
    else {
        panic("Double add failure in GrassMeshManager::trackChunk");
    }
}

void GrassMeshManager::stopTrackingChunk(ChunkID chunkId) {
    std::lock_guard lock(mTrackedChunksMutex);
    auto&& it = mTrackedChunksLookup.find(chunkId);
    if (it != mTrackedChunksLookup.end()) {
        TrackedChunkLookupData& data = it->second;
        // While we are active, we have a ref, so this should be impossible
        assert(!data.isActive);
        mTrackedChunksLookup[mTrackedChunks.back().chunk->getChunkID()].index = data.index;
        mTrackedChunks[data.index] = std::move(mTrackedChunks.back());
        mTrackedChunks.pop_back();
        mTrackedChunksLookup.erase(it);
    }
}
