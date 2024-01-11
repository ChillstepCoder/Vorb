#pragma once


#include "boost/container/flat_set.hpp"
#include "tile/TileContainer.h"

#include "world/ChunkEvents.h"
#include "rendering/mesh/TileGrassMeshType.h"

#include "util/ThreadSafeDirtySet.h"
#include <boost/functional/hash.hpp> // For boost::hash_combine

class ChunkGrassQuadtree;
class Chunk;
class World;
class GrassMesh;

typedef std::pair<TileContainerEventDispatcher::Handle, ChunkEventDispatcher::Handle> GrassEventPair;

// Specialize std::hash for std::pair<ChunkID, i16v2>
namespace std {
    template<>
    struct hash<std::pair<ChunkID, i16v2>> {
        size_t operator()(const std::pair<ChunkID, i16v2>& p) const {
            size_t seed = 0;
            boost::hash_combine(seed, std::hash<ChunkID>()(p.first));
            boost::hash_combine(seed, std::hash<i16v2>()(p.second));
            return seed;
        }
    };
}

// Shared by game + render thread
class GrassMeshManager
{
public:
    GrassMeshManager(World& world);
    ~GrassMeshManager();

    void shutdown();

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
    bool mDidShutDown = false;

    ThreadSafeDirtySet<std::pair<ChunkID, i16v2>> mDirtyPositions;

};

