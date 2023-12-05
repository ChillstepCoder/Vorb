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
        mTrackedChunksLookup.emplace(chunkId, TrackedChunk{ nullptr, &chunk, chunk.getWorldPosCenter2D() });
        // Const cast ~ get fucked
        Chunk& chunkNonConst = const_cast<Chunk&>(chunk);
        TileContainer* container = chunkNonConst.getTileContainer();
        mTileEditEventHandles[container->getId()] = std::make_pair(
            container->addEditTilesListener([this](const TileContainerEvent& evnt) {
                PROFILE_SCOPE("GrassEdit Dirty");
                ASSERT_GAME_THREAD();

                const TileContainerEditEvent& editEvent = std::get<TileContainerEditEvent>(evnt.varEvent);

                if (editEvent.type == TileContainerEditEventType::ChangeZPos) {

                    std::lock_guard lock(mTrackedChunksMutex);
                    auto&& it = mTrackedChunksLookup.find(evnt.container->getOwnerChunk()->getChunkID());
                    if (it != mTrackedChunksLookup.end() && it->second.second) {
                        for (ui32 i = 0; i < editEvent.editCount; ++i) {
                            assert(owner);
                            TileContainerEditZPosEventData& data = editEvent.changeZPosArray[i];
                            quadtreePtr->markDirty(f32v2(data.worldPosition));
                        }
                    }
                }
            }),
            chunkNonConst.addGrassEditListener([this](const ChunkEvent& evnt) {
                auto& quadtreePtr = mChunkGrassQuadtrees[&evnt.chunk];
                if (quadtreePtr) {
                    const f32v2 worldPos = evnt.chunk.getTileContainer()->getTileSpatialGrid().getTileBaseWorldPos2D(evnt.tileIndex);
                    quadtreePtr->markDirty(worldPos);
                }
            })
        );
        // Destroy will be handled by removeGrassForChunk
    }
    else {
        panic("Double add failure in GrassMeshManager::trackChunk");
    }
}

void GrassMeshManager::stopTrackingChunk(ChunkID chunkId) {
    std::lock_guard lock(mTrackedChunksMutex);
    const Chunk& chunk = mWorld.getChunkGrid().getChunk(chunkId)
    // If we never existed, return
    if (!chunk.getTileContainer()) {
        return;
    }
    auto&& it = mTileEditEventHandles.find(chunk.getTileContainer()->getId());
    if (it != mTileEditEventHandles.end()) {
        // Const cast ~ get fucked
        Chunk& chunkNonConst = const_cast<Chunk&>(chunk);
        TileContainer* container = chunkNonConst.getTileContainer();
        // TODO: Can this be automatic? We are only holding a weak_ptr handle...
        container->removeEditTilesListener(it->second.first);
        chunkNonConst.removeGrassEditListener(it->second.second);
        mTileEditEventHandles.erase(it);
    }
   
    // TODO: uhhh....
    //auto&& it = mChunkGrassQuadtrees.find(&chunk);
    //if (it != mChunkGrassQuadtrees.end()) {
    //    // If we hit this, it means that we didnt reset it in tick above before removing..
    //    assert(it->second == nullptr);
    //    mChunkGrassQuadtrees.erase(it);
    //}
}

void GrassMeshManager::dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) {
    ASSERT_GAME_THREAD();
    // Only update grass which was impacted by brush
    for (auto&& quadtree : mChunkGrassQuadtrees) {
        if (quadtree.second) {
            quadtree.second->onDataChanged(pos, brushRadius);
        }
    }
}