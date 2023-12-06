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

    struct TrackedChunk {
        std::unique_ptr<ChunkGrassQuadtree> quadtree;
        const Chunk* chunk;
        f32v2 worldPosCenter;
    };
    void frameUpdate(const f32v2& loadCenter, f32 elapsedSec);

    void addGrassMesh(const GrassMesh* mesh) { ASSERT_RENDER_THREAD(); mGrassMeshes.insert(mesh); }
    void removeGrassMesh(const GrassMesh* mesh) { ASSERT_RENDER_THREAD(); mGrassMeshes.erase(mesh); }

    const std::vector<TrackedChunk>& getTrackedChunks() const { ASSERT_RENDER_THREAD(); return mTrackedChunks; }
    const boost::container::flat_set<const GrassMesh*>& getGrassMeshes() const { ASSERT_RENDER_THREAD(); return mGrassMeshes; }

private:
    void trackChunk(ChunkID chunkId);
    void stopTrackingChunk(ChunkID chunkId);
    boost::container::flat_set<const GrassMesh*> mGrassMeshes;

    struct TrackedChunkLookupData {
        ui32 index;
        bool isActive;
        GrassEventPair eventHandle;
    };

    std::vector<TrackedChunk> mTrackedChunks;
    std::mutex mTrackedChunksMutex;
    std::unordered_map<ChunkID, TrackedChunkLookupData> mTrackedChunksLookup;

    World& mWorld;
    moodycamel::ConcurrentQueue<std::pair<ChunkID, bool /*startTracking*/>> mChunkTrackChanges;
    moodycamel::ConcurrentQueue<f32v2> mTileContainerEdits;

};

