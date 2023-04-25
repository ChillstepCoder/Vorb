#include "stdafx.h"
#include "GrassMeshManager.h"

#include "rendering/ChunkGrassQuadtree.h"

#include "world/IWorld.h"
#include "world/Chunk.h"

#include "options/DebugOptions.h"

GrassMeshManager::GrassMeshManager() {
  
}

GrassMeshManager::~GrassMeshManager() {

}

void GrassMeshManager::tick() {
    assert(IS_GAME_THREAD());
    PROFILE_FUNCTION();

    const f32v2& loadCenter = sMainGameWorld->getLoadCenter();

    // Update grass
    for (auto&& it : mChunkGrassQuadtrees) {
        const Chunk& chunk = *it.first;
        if (chunk.isDataReady()) {

            const f32 distSq = glm::length2(chunk.getWorldPosCenter2D() - loadCenter);

            std::unique_ptr<ChunkGrassQuadtree>& grassQuadtree = it.second;
            if (grassQuadtree) {
                grassQuadtree->update(loadCenter);
                if (distSq > sDebugOptions.mGrassSettings.distanceSq + 10.0f) {
                    if (grassQuadtree->getRefCount() == 0) {
                        grassQuadtree.reset();
                    }
                }
                else {
                    grassQuadtree->update(loadCenter);
                }
            }
            else if (distSq < sDebugOptions.mGrassSettings.distanceSq) {
                grassQuadtree = std::make_unique<ChunkGrassQuadtree>(chunk);
            }
        }
    }
}

void GrassMeshManager::addGrassForChunk(const Chunk& chunk) {
    assert(IS_GAME_THREAD());
    auto&& it = mChunkGrassQuadtrees.find(&chunk);
    if (it == mChunkGrassQuadtrees.end()) {
        mChunkGrassQuadtrees[&chunk] = nullptr;
         // Const cast ~ get fucked
        Chunk& chunkNonConst = const_cast<Chunk&>(chunk);
        TileContainer* container = chunkNonConst.getTileContainer();
        mEditEventHandles[container->getId()] = container->addEditTilesListener([this](const TileContainerEvent& evnt) {
            PROFILE_SCOPE("GrassEdit Dirty");
            assert(IS_GAME_THREAD());
            if (evnt.edit.type == TileContainerEditEventType::ChangeZPos) {
                const Chunk* owner = evnt.container->getOwnerChunk();
                auto& quadtreePtr = mChunkGrassQuadtrees[owner];
                if (quadtreePtr) {
                    for (ui32 i = 0; i < evnt.edit.editCount; ++i) {
                        assert(owner);
                        TileContainerEditZPosEventData& data = evnt.edit.changeZPosArray[i];
                        quadtreePtr->markDirty(f32v2(data.worldPosition));
                    }
                }
            }
        });
        // Destroy will be handled by removeGrassForChunk
    }
}

void GrassMeshManager::removeGrassForChunk(const Chunk& chunk) {
    assert(IS_GAME_THREAD());
    assert(chunk.getTileContainer());
    auto&& it = mEditEventHandles.find(chunk.getTileContainer()->getId());
    assert(it != mEditEventHandles.end());
    // Const cast ~ get fucked
    Chunk& chunkNonConst = const_cast<Chunk&>(chunk);
    TileContainer* container = chunkNonConst.getTileContainer();
    // TODO: Can this be automatic? We are only holding a weak_ptr handle...
    container->removeEditTilesListener(it->second);
    mEditEventHandles.erase(it); // TODO: CRASH HERE WHEN TELEPORTING FAR AWAY
   
    // TODO: uhhh....
    //auto&& it = mChunkGrassQuadtrees.find(&chunk);
    //if (it != mChunkGrassQuadtrees.end()) {
    //    // If we hit this, it means that we didnt reset it in tick above before removing..
    //    assert(it->second == nullptr);
    //    mChunkGrassQuadtrees.erase(it);
    //}
}

void GrassMeshManager::dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) {
    assert(IS_GAME_THREAD());
    // Only update grass which was impacted by brush
    for (auto&& quadtree : mChunkGrassQuadtrees) {
        if (quadtree.second) {
            quadtree.second->onDataChanged(pos, brushRadius);
        }
    }
}