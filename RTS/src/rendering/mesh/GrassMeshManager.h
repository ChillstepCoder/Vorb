#pragma once


#include "boost/container/flat_set.hpp"
#include "tile/TileContainer.h"

#include "world/ChunkEvents.h"
#include "rendering/mesh/TileGrassMeshType.h"

class ChunkGrassQuadtree;
class Chunk;
class IWorld;
class GrassMesh;

typedef std::pair<TileContainerEventDispatcher::Handle, ChunkEventDispatcher::Handle> GrassEventPair;

// Shared by game + render thread
class GrassMeshManager
{
public:
    GrassMeshManager(IWorld& world);
    ~GrassMeshManager();

    void tickGameThread(const f32v2& loadCenter);
    void addGrassForChunk(const Chunk& chunk);
    void removeGrassForChunk(const Chunk& chunk);
    void dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius);

    void addGrassMesh(const GrassMesh* mesh) { ASSERT_RENDER_THREAD(); mGrassMeshes.insert(mesh); }
    void removeGrassMesh(const GrassMesh* mesh) { ASSERT_RENDER_THREAD(); mGrassMeshes.erase(mesh); }

    const std::map<const Chunk*, std::unique_ptr<ChunkGrassQuadtree>>& getGrassQuadtrees() const { ASSERT_GAME_THREAD(); return mChunkGrassQuadtrees; }
    const boost::container::flat_set<const GrassMesh*>& getGrassMeshes() const { ASSERT_RENDER_THREAD(); return mGrassMeshes; }

private:
    boost::container::flat_set<const GrassMesh*> mGrassMeshes;

    std::map<const Chunk*, std::unique_ptr<ChunkGrassQuadtree>> mChunkGrassQuadtrees;
    boost::container::flat_map<TileContainerID, GrassEventPair> mTileEditEventHandles;
};

