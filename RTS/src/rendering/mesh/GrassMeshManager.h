#pragma once


#include "boost/container/flat_set.hpp"
#include "tile/TileContainer.h"

#include "world/ChunkEvents.h"
#include "rendering/mesh/TileGrassMeshType.h"

class ChunkGrassQuadtree;
class Chunk;
class World;
class GrassMesh;

typedef std::pair<TileContainerEventDispatcher::Handle, ChunkEventDispatcher::Handle> GrassEventPair;

// Shared by game + render thread
class GrassMeshManager
{
public:
    GrassMeshManager(World& world);
    ~GrassMeshManager();

    void frameUpdate(const f32v2& loadCenter, f32 elapsedSec);
    void dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius);

    void addGrassMesh(const GrassMesh* mesh) { ASSERT_RENDER_THREAD(); mGrassMeshes.insert(mesh); }
    void removeGrassMesh(const GrassMesh* mesh) { ASSERT_RENDER_THREAD(); mGrassMeshes.erase(mesh); }

    const std::map<const Chunk*, std::unique_ptr<ChunkGrassQuadtree>>& getGrassQuadtrees() const { ASSERT_GAME_THREAD(); return mChunkGrassQuadtrees; }
    const boost::container::flat_set<const GrassMesh*>& getGrassMeshes() const { ASSERT_RENDER_THREAD(); return mGrassMeshes; }

private:
    void trackChunk(ChunkID chunkId);
    void stopTrackingChunk(ChunkID chunkId);
    boost::container::flat_set<const GrassMesh*> mGrassMeshes;

    struct TrackedChunk {
        std::unique_ptr<ChunkGrassQuadtree> quadtree;
        const Chunk* chunk;
        f32v2 worldPosCenter;
    };

    std::vector<TrackedChunk> mTrackedChunks;
    std::unordered_map<ChunkID, ui32> mTrackedChunksLookup;

    boost::container::flat_map<TileContainerID, GrassEventPair> mTileEditEventHandles;

    World& mWorld;
    moodycamel::ConcurrentQueue<std::pair<ChunkID, bool /*startTracking*/>> mChunkTrackChanges;
};

